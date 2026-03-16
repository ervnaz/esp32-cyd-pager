// =============================================================================
//  espnow_manager.h  —  ESP-NOW send/receive + peer management
// =============================================================================
#pragma once
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "message_types.h"
#include "config.h"

namespace ESPNowManager {

  static PeerInfo   _peers[MAX_PEERS];
  static int        _peer_count  = 0;
  static uint32_t   _msg_counter = 0;
  static uint32_t   _last_hb_ms  = 0;
  static bool       _send_ok     = false;

  static void (*_rx_cb)(const PagerMessage&) = nullptr;

  static bool _macsEqual(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, 6) == 0;
  }

  // Get actual WiFi channel (so Arduino and ESPHome units share the same channel)
  static uint8_t _wifiChannel() {
    uint8_t primary;
    wifi_second_chan_t second;
    esp_wifi_get_channel(&primary, &second);
    return primary > 0 ? primary : ESPNOW_CHANNEL;
  }

  static void _registerBroadcastPeer() {
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, BROADCAST_MAC, 6);
    peer.channel = _wifiChannel();
    peer.encrypt = false;
    if (!esp_now_is_peer_exist(BROADCAST_MAC))
      esp_now_add_peer(&peer);
  }

  static void _upsertPeer(const uint8_t* mac, const char* name) {
    for (int i = 0; i < _peer_count; i++) {
      if (_macsEqual(_peers[i].mac, mac)) {
        _peers[i].last_seen_ms = millis();
        _peers[i].active = true;
        strncpy(_peers[i].name, name, DEVICE_NAME_LEN - 1);
        return;
      }
    }
    if (_peer_count >= MAX_PEERS) return;
    memcpy(_peers[_peer_count].mac, mac, 6);
    strncpy(_peers[_peer_count].name, name, DEVICE_NAME_LEN - 1);
    _peers[_peer_count].name[DEVICE_NAME_LEN - 1] = '\0';
    _peers[_peer_count].last_seen_ms = millis();
    _peers[_peer_count].active = true;
    _peer_count++;

    esp_now_peer_info_t pi{};
    memcpy(pi.peer_addr, mac, 6);
    pi.channel = _wifiChannel();
    pi.encrypt = false;
    if (!esp_now_is_peer_exist(mac)) esp_now_add_peer(&pi);

    Serial.printf("[ESPNOW] New peer: %s\n", name);
  }

  static void _onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < (int)sizeof(EspNowPacket)) return;
    const EspNowPacket* pkt = reinterpret_cast<const EspNowPacket*>(data);

    uint8_t my_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, my_mac);
    if (_macsEqual(pkt->sender_mac, my_mac)) return;

    _upsertPeer(pkt->sender_mac, pkt->sender_name);

    if (pkt->type == PKT_HEARTBEAT) return;

    if (pkt->type == PKT_MESSAGE && _rx_cb) {
      PagerMessage msg{};
      msg.id          = pkt->msg_id;
      msg.timestamp_s = pkt->timestamp_s;
      memcpy(msg.sender_mac, pkt->sender_mac, 6);
      strncpy(msg.sender_name, pkt->sender_name, DEVICE_NAME_LEN - 1);
      strncpy(msg.body, pkt->body, MAX_MSG_LEN - 1);
      msg.is_mine      = false;
      msg.ack_received = false;
      _rx_cb(msg);

      // Send ACK
      EspNowPacket ack{};
      ack.type = PKT_ACK;
      esp_wifi_get_mac(WIFI_IF_STA, ack.sender_mac);
      strncpy(ack.sender_name, DEVICE_NAME, DEVICE_NAME_LEN - 1);
      ack.msg_id = pkt->msg_id;
      esp_now_send(pkt->sender_mac, (uint8_t*)&ack, sizeof(ack));
    }

    if (pkt->type == PKT_ACK) {
      Serial.printf("[ESPNOW] ACK for msg %u\n", pkt->msg_id);
      // markAck posted via main loop flag to avoid cross-task issues
    }
  }

  static void _onSent(const uint8_t* mac, esp_now_send_status_t status) {
    _send_ok = (status == ESP_NOW_SEND_SUCCESS);
  }

  inline void begin(void (*rx_callback)(const PagerMessage&)) {
    _rx_cb = rx_callback;
    if (esp_now_init() != ESP_OK) {
      Serial.println("[ESPNOW] Init failed!");
      return;
    }
    esp_now_register_recv_cb(_onRecv);
    esp_now_register_send_cb(_onSent);
    _registerBroadcastPeer();
    Serial.printf("[ESPNOW] Ready on channel %d\n", _wifiChannel());
  }

  inline bool sendMessage(const char* text) {
    EspNowPacket pkt{};
    pkt.type = PKT_MESSAGE;
    esp_wifi_get_mac(WIFI_IF_STA, pkt.sender_mac);
    strncpy(pkt.sender_name, DEVICE_NAME, DEVICE_NAME_LEN - 1);
    pkt.msg_id      = ++_msg_counter;
    pkt.timestamp_s = millis() / 1000;
    strncpy(pkt.body, text, MAX_MSG_LEN - 1);

    esp_err_t r = esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
    if (r != ESP_OK) {
      Serial.printf("[ESPNOW] Send error: %d\n", r);
      return false;
    }

    // Store locally as sent message
    PagerMessage sent{};
    sent.id          = pkt.msg_id;
    sent.timestamp_s = pkt.timestamp_s;
    strncpy(sent.sender_name, DEVICE_NAME, DEVICE_NAME_LEN - 1);
    esp_wifi_get_mac(WIFI_IF_STA, sent.sender_mac);
    strncpy(sent.body, text, MAX_MSG_LEN - 1);
    sent.is_mine      = true;
    sent.ack_received = false;
    if (_rx_cb) _rx_cb(sent);

    return true;
  }

  inline void update() {
    uint32_t now = millis();
    if (now - _last_hb_ms > HEARTBEAT_MS) {
      _last_hb_ms = now;
      EspNowPacket hb{};
      hb.type = PKT_HEARTBEAT;
      esp_wifi_get_mac(WIFI_IF_STA, hb.sender_mac);
      strncpy(hb.sender_name, DEVICE_NAME, DEVICE_NAME_LEN - 1);
      hb.msg_id      = 0;
      hb.timestamp_s = now / 1000;
      esp_now_send(BROADCAST_MAC, (uint8_t*)&hb, sizeof(hb));
    }
    for (int i = 0; i < _peer_count; i++) {
      if (_peers[i].active && (now - _peers[i].last_seen_ms > PEER_TIMEOUT_MS)) {
        _peers[i].active = false;
        Serial.printf("[ESPNOW] Peer timed out: %s\n", _peers[i].name);
      }
    }
  }

  inline int       peerCount()        { return _peer_count; }
  inline PeerInfo* getPeer(int i)     { return (i < _peer_count) ? &_peers[i] : nullptr; }
  inline bool      lastSendOk()       { return _send_ok; }
}

#pragma once
// =============================================================================
//  espnow_bridge.h  —  ESP-NOW glue for ESPHome (IDF framework)
//  Place this file in the same directory as pager-node.yaml
// =============================================================================
#include "esp_netif.h"      // must come before esp_wifi headers
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <string>
#include <cstring>

// ── Device name — set via ESPHome substitution in the yaml build_flags ───────
#ifndef PAGER_DEVICE_NAME
  #define PAGER_DEVICE_NAME "UNIT-1"
#endif

// ── Constants ────────────────────────────────────────────────────────────────
#define ESPNOW_CHANNEL   1
#define MAX_MSG_LEN      200
#define DEVICE_NAME_LEN  9
#define MAX_PEERS        10
#define PEER_TIMEOUT_MS  30000
#define HEARTBEAT_MS     5000

static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// ── Shared state (read by YAML lambdas) ──────────────────────────────────────
int         espnow_peer_count     = 0;
int         espnow_unread_count   = 0;
std::string espnow_last_body      = "";
std::string espnow_last_sender    = "";
std::string espnow_last_sent      = "";
bool        espnow_notify_visible = false;
uint32_t    espnow_notify_start_ms = 0;

// ── Wire packet ──────────────────────────────────────────────────────────────
enum PktType : uint8_t {
  PKT_MESSAGE   = 0x01,
  PKT_HEARTBEAT = 0x02,
  PKT_ACK       = 0x03,
};

struct __attribute__((packed)) EspNowPacket {
  PktType  type;
  uint8_t  sender_mac[6];
  char     sender_name[DEVICE_NAME_LEN];
  uint32_t msg_id;
  uint32_t timestamp_s;
  char     body[MAX_MSG_LEN];
};

// ── Peer table ────────────────────────────────────────────────────────────────
struct PeerEntry {
  uint8_t  mac[6];
  char     name[DEVICE_NAME_LEN];
  uint32_t last_seen_ms;
  bool     active;
};
static PeerEntry _peers[MAX_PEERS];
static int       _peer_total  = 0;
static uint32_t  _msg_counter = 0;
static uint32_t  _last_hb_ms  = 0;

static bool _macsEqual(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static void _recount_peers() {
  int active = 0;
  for (int i = 0; i < _peer_total; i++)
    if (_peers[i].active) active++;
  espnow_peer_count = active;
}

static void _upsertPeer(const uint8_t* mac, const char* name) {
  for (int i = 0; i < _peer_total; i++) {
    if (_macsEqual(_peers[i].mac, mac)) {
      _peers[i].last_seen_ms = millis();
      _peers[i].active = true;
      _recount_peers();
      return;
    }
  }
  if (_peer_total >= MAX_PEERS) return;
  memcpy(_peers[_peer_total].mac, mac, 6);
  strncpy(_peers[_peer_total].name, name, DEVICE_NAME_LEN - 1);
  _peers[_peer_total].name[DEVICE_NAME_LEN - 1] = '\0';
  _peers[_peer_total].last_seen_ms = millis();
  _peers[_peer_total].active = true;
  _peer_total++;

  esp_now_peer_info_t pi{};
  memcpy(pi.peer_addr, mac, 6);
  // Use actual WiFi channel
  uint8_t primary; wifi_second_chan_t second;
  esp_wifi_get_channel(&primary, &second);
  pi.channel = primary;
  pi.encrypt = false;
  if (!esp_now_is_peer_exist(mac)) esp_now_add_peer(&pi);

  _recount_peers();
}

// ── Receive callback — new IDF signature uses esp_now_recv_info_t* ───────────
static void _onRecv(const esp_now_recv_info_t* recv_info,
                    const uint8_t* data, int len) {
  if (len < (int)sizeof(EspNowPacket)) return;
  const EspNowPacket* pkt = reinterpret_cast<const EspNowPacket*>(data);

  // Ignore our own looped-back broadcasts
  uint8_t my_mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, my_mac);
  if (_macsEqual(pkt->sender_mac, my_mac)) return;

  _upsertPeer(pkt->sender_mac, pkt->sender_name);

  if (pkt->type == PKT_HEARTBEAT) return;

  if (pkt->type == PKT_MESSAGE) {
    espnow_last_sender    = std::string(pkt->sender_name);
    espnow_last_body      = std::string(pkt->body);
    espnow_unread_count++;
    espnow_notify_visible  = true;
    espnow_notify_start_ms = millis();

    // Send ACK
    EspNowPacket ack{};
    ack.type = PKT_ACK;
    esp_wifi_get_mac(WIFI_IF_STA, ack.sender_mac);
    strncpy(ack.sender_name, PAGER_DEVICE_NAME, DEVICE_NAME_LEN - 1);
    ack.msg_id = pkt->msg_id;
    esp_now_send(pkt->sender_mac, (uint8_t*)&ack, sizeof(ack));
  }
}

// ── Send callback — new IDF signature uses wifi_tx_info_t* ───────────────────
static void _onSent(const wifi_tx_info_t* tx_info,
                    esp_now_send_status_t status) {
  // Optionally surface status to a binary_sensor here
  (void)tx_info;
  (void)status;
}

// ── Public: broadcast a text message ─────────────────────────────────────────
void espnow_send_message(const char* text) {
  EspNowPacket pkt{};
  pkt.type = PKT_MESSAGE;
  esp_wifi_get_mac(WIFI_IF_STA, pkt.sender_mac);
  strncpy(pkt.sender_name, PAGER_DEVICE_NAME, DEVICE_NAME_LEN - 1);
  pkt.msg_id      = ++_msg_counter;
  pkt.timestamp_s = millis() / 1000;
  strncpy(pkt.body, text, MAX_MSG_LEN - 1);
  pkt.body[MAX_MSG_LEN - 1] = '\0';
  esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
  espnow_last_sent = std::string(text);
}

// ── Init — call once from on_boot lambda ─────────────────────────────────────
void espnow_bridge_init() {
  // Ensure WiFi is in STA mode before init
  wifi_mode_t mode;
  if (esp_wifi_get_mode(&mode) != ESP_OK || mode == WIFI_MODE_NULL) {
    ESP_LOGE("espnow", "WiFi not ready — skipping ESP-NOW init");
    return;
  }

  // Use the actual WiFi channel so ESP-NOW peers match
  uint8_t primary;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&primary, &second);
  ESP_LOGI("espnow", "Using WiFi channel %d for ESP-NOW", primary);

  if (esp_now_init() != ESP_OK) {
    ESP_LOGE("espnow", "ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(_onRecv);
  esp_now_register_send_cb(_onSent);

  // Register broadcast peer using actual WiFi channel
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, BROADCAST_MAC, 6);
  peer.channel = primary;   // match actual WiFi channel
  peer.encrypt = false;
  if (!esp_now_is_peer_exist(BROADCAST_MAC)) esp_now_add_peer(&peer);

  ESP_LOGI("espnow", "ESP-NOW bridge ready — device: %s, channel: %d",
           PAGER_DEVICE_NAME, ESPNOW_CHANNEL);
}

// ── Loop — call every second from interval: lambda ───────────────────────────
void espnow_bridge_loop() {
  uint32_t now = millis();

  // Heartbeat
  if (now - _last_hb_ms > HEARTBEAT_MS) {
    _last_hb_ms = now;
    EspNowPacket hb{};
    hb.type = PKT_HEARTBEAT;
    esp_wifi_get_mac(WIFI_IF_STA, hb.sender_mac);
    strncpy(hb.sender_name, PAGER_DEVICE_NAME, DEVICE_NAME_LEN - 1);
    hb.msg_id      = 0;
    hb.timestamp_s = now / 1000;
    esp_now_send(BROADCAST_MAC, (uint8_t*)&hb, sizeof(hb));
  }

  // Expire stale peers
  for (int i = 0; i < _peer_total; i++) {
    if (_peers[i].active && (now - _peers[i].last_seen_ms > PEER_TIMEOUT_MS))
      _peers[i].active = false;
  }
  _recount_peers();

  // Dismiss notification after 3 s
  if (espnow_notify_visible && (now - espnow_notify_start_ms > 3000))
    espnow_notify_visible = false;
}

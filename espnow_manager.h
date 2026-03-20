// =============================================================================
//  espnow_manager.h  —  Standalone ESP-NOW mesh, no WiFi router required
// =============================================================================
#pragma once
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_random.h>
#include <WiFi.h>
#include <Preferences.h>
#include "message_types.h"
#include "config.h"

static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const uint8_t _EN_PMK[16] = { PAGER_PMK_BYTES };
static const uint8_t _EN_LMK[16] = { PAGER_LMK_BYTES };

// ── X25519 public key from private (no mbedTLS) ───────────────────────────────
static void _en_x25519(uint8_t pub[32], const uint8_t priv[32]) {
  typedef int64_t fe[10];
  static const uint8_t BASE[32] = {9};
  auto fc=[](fe o,const fe i){for(int j=0;j<10;j++)o[j]=i[j];};
  auto fs=[](fe f,fe g,uint32_t b){b=(uint32_t)(-(int32_t)b);for(int i=0;i<10;i++){int64_t x=(f[i]^g[i])&(int64_t)b;f[i]^=x;g[i]^=x;}};
  auto fa=[](fe o,const fe a,const fe b){for(int i=0;i<10;i++)o[i]=a[i]+b[i];};
  auto fb=[](fe o,const fe a,const fe b){for(int i=0;i<10;i++)o[i]=a[i]-b[i];};
  auto ff=[](fe h,const uint8_t* s){
    h[0]=(int64_t)(s[0]|((uint32_t)s[1]<<8)|((uint32_t)s[2]<<16)|((uint32_t)(s[3]&0xf)<<24));
    h[1]=(int64_t)((s[3]>>4)|((uint32_t)s[4]<<4)|((uint32_t)s[5]<<12)|((uint32_t)s[6]<<20));
    h[2]=(int64_t)(s[7]|((uint32_t)s[8]<<8)|((uint32_t)s[9]<<16)|((uint32_t)(s[10]&0xf)<<24));
    h[3]=(int64_t)((s[10]>>4)|((uint32_t)s[11]<<4)|((uint32_t)s[12]<<12)|((uint32_t)s[13]<<20));
    h[4]=(int64_t)(s[14]|((uint32_t)s[15]<<8)|((uint32_t)s[16]<<16)|((uint32_t)(s[17]&0xf)<<24));
    h[5]=(int64_t)((s[17]>>4)|((uint32_t)s[18]<<4)|((uint32_t)s[19]<<12)|((uint32_t)s[20]<<20));
    h[6]=(int64_t)(s[21]|((uint32_t)s[22]<<8)|((uint32_t)s[23]<<16)|((uint32_t)(s[24]&0xf)<<24));
    h[7]=(int64_t)((s[24]>>4)|((uint32_t)s[25]<<4)|((uint32_t)s[26]<<12)|((uint32_t)s[27]<<20));
    h[8]=(int64_t)(s[28]|((uint32_t)s[29]<<8)|((uint32_t)s[30]<<16)|((uint32_t)(s[31]&0xf)<<24));
    h[9]=(int64_t)((s[31]>>4)&0x0fffffff);};
  auto fm=[](fe h,const fe f,const fe g){
    int64_t f0=f[0],f1=f[1],f2=f[2],f3=f[3],f4=f[4],f5=f[5],f6=f[6],f7=f[7],f8=f[8],f9=f[9];
    int64_t g0=g[0],g1=g[1],g2=g[2],g3=g[3],g4=g[4],g5=g[5],g6=g[6],g7=g[7],g8=g[8],g9=g[9];
    int64_t g1x=19*g1,g2x=19*g2,g3x=19*g3,g4x=19*g4,g5x=19*g5,g6x=19*g6,g7x=19*g7,g8x=19*g8,g9x=19*g9;
    h[0]=f0*g0+f1*g9x+f2*g8x+f3*g7x+f4*g6x+f5*g5x+f6*g4x+f7*g3x+f8*g2x+f9*g1x;
    h[1]=f0*g1+f1*g0+f2*g9x+f3*g8x+f4*g7x+f5*g6x+f6*g5x+f7*g4x+f8*g3x+f9*g2x;
    h[2]=f0*g2+f1*g1*2+f2*g0+f3*g9x+f4*g8x+f5*g7x+f6*g6x+f7*g5x+f8*g4x+f9*g3x;
    h[3]=f0*g3+f1*g2+f2*g1+f3*g0+f4*g9x+f5*g8x+f6*g7x+f7*g6x+f8*g5x+f9*g4x;
    h[4]=f0*g4+f1*g3*2+f2*g2+f3*g1*2+f4*g0+f5*g9x+f6*g8x+f7*g7x+f8*g6x+f9*g5x;
    h[5]=f0*g5+f1*g4+f2*g3+f3*g2+f4*g1+f5*g0+f6*g9x+f7*g8x+f8*g7x+f9*g6x;
    h[6]=f0*g6+f1*g5*2+f2*g4+f3*g3*2+f4*g2+f5*g1*2+f6*g0+f7*g9x+f8*g8x+f9*g7x;
    h[7]=f0*g7+f1*g6+f2*g5+f3*g4+f4*g3+f5*g2+f6*g1+f7*g0+f8*g9x+f9*g8x;
    h[8]=f0*g8+f1*g7*2+f2*g6+f3*g5*2+f4*g4+f5*g3*2+f6*g2+f7*g1*2+f8*g0+f9*g9x;
    h[9]=f0*g9+f1*g8+f2*g7+f3*g6+f4*g5+f5*g4+f6*g3+f7*g2+f8*g1+f9*g0;
    int64_t c;
    c=h[0]>>26;h[1]+=c;h[0]-=c<<26; c=h[1]>>25;h[2]+=c;h[1]-=c<<25;
    c=h[2]>>26;h[3]+=c;h[2]-=c<<26; c=h[3]>>25;h[4]+=c;h[3]-=c<<25;
    c=h[4]>>26;h[5]+=c;h[4]-=c<<26; c=h[5]>>25;h[6]+=c;h[5]-=c<<25;
    c=h[6]>>26;h[7]+=c;h[6]-=c<<26; c=h[7]>>25;h[8]+=c;h[7]-=c<<25;
    c=h[8]>>26;h[9]+=c;h[8]-=c<<26; c=h[9]>>25;h[0]+=c*19;h[9]-=c<<25;
    c=h[0]>>26;h[1]+=c;h[0]-=c<<26;};
  auto ft=[](uint8_t* s,const fe h){
    int64_t h0=h[0],h1=h[1],h2=h[2],h3=h[3],h4=h[4],h5=h[5],h6=h[6],h7=h[7],h8=h[8],h9=h[9];
    int64_t q=(19*h9+(1<<24))>>25;
    q=(h0+q)>>26;q=(h1+q)>>25;q=(h2+q)>>26;q=(h3+q)>>25;
    q=(h4+q)>>26;q=(h5+q)>>25;q=(h6+q)>>26;q=(h7+q)>>25;
    q=(h8+q)>>26;q=(h9+q)>>25; h0+=19*q;
    int64_t c=h0>>26;h1+=c;h0-=c<<26; c=h1>>25;h2+=c;h1-=c<<25;
    c=h2>>26;h3+=c;h2-=c<<26; c=h3>>25;h4+=c;h3-=c<<25;
    c=h4>>26;h5+=c;h4-=c<<26; c=h5>>25;h6+=c;h5-=c<<25;
    c=h6>>26;h7+=c;h6-=c<<26; c=h7>>25;h8+=c;h7-=c<<25;
    c=h8>>26;h9+=c;h8-=c<<26;
    s[0]=(uint8_t)h0;s[1]=(uint8_t)(h0>>8);s[2]=(uint8_t)(h0>>16);
    s[3]=(uint8_t)((h0>>24)|(h1<<2));s[4]=(uint8_t)(h1>>6);s[5]=(uint8_t)(h1>>14);
    s[6]=(uint8_t)((h1>>22)|(h2<<3));s[7]=(uint8_t)(h2>>5);s[8]=(uint8_t)(h2>>13);
    s[9]=(uint8_t)((h2>>21)|(h3<<5));s[10]=(uint8_t)(h3>>3);s[11]=(uint8_t)(h3>>11);
    s[12]=(uint8_t)((h3>>19)|(h4<<6));s[13]=(uint8_t)(h4>>2);s[14]=(uint8_t)(h4>>10);
    s[15]=(uint8_t)(h4>>18);
    s[16]=(uint8_t)h5;s[17]=(uint8_t)(h5>>8);s[18]=(uint8_t)(h5>>16);
    s[19]=(uint8_t)((h5>>24)|(h6<<1));s[20]=(uint8_t)(h6>>7);s[21]=(uint8_t)(h6>>15);
    s[22]=(uint8_t)((h6>>23)|(h7<<3));s[23]=(uint8_t)(h7>>5);s[24]=(uint8_t)(h7>>13);
    s[25]=(uint8_t)((h7>>21)|(h8<<4));s[26]=(uint8_t)(h8>>4);s[27]=(uint8_t)(h8>>12);
    s[28]=(uint8_t)((h8>>20)|(h9<<6));s[29]=(uint8_t)(h9>>2);s[30]=(uint8_t)(h9>>10);
    s[31]=(uint8_t)(h9>>18);};
  uint8_t e[32]; memcpy(e,priv,32);
  e[0]&=248;e[31]&=127;e[31]|=64;
  fe x1,x2,z2,x3,z3; ff(x1,BASE);
  fe one={1};fc(x2,one);fe zero={0};fc(z2,zero);fc(x3,x1);fc(z3,one);
  uint32_t sw=0;
  for(int pos=254;pos>=0;pos--){
    uint32_t b=(e[pos/8]>>(pos&7))&1; sw^=b; fs(x2,x3,sw); fs(z2,z3,sw); sw=b;
    fe A,AA,B,BB,E,C,D,DA,CB;
    fa(A,x2,z2);fm(AA,A,A);fb(B,x2,z2);fm(BB,B,B);
    fb(E,AA,BB);fa(C,x3,z3);fb(D,x3,z3);
    fm(DA,D,A);fm(CB,C,B);
    fa(x3,DA,CB);fm(x3,x3,x3);fb(z3,DA,CB);fm(z3,z3,z3);fm(z3,z3,x1);
    fm(x2,AA,BB);fe a24={121665},t;fm(t,E,a24);fa(t,t,AA);fm(z2,E,t);}
  fs(x2,x3,sw);fs(z2,z3,sw);
  fe t0,t1,t2,t3;
  fm(t0,z2,z2);fm(t1,t0,z2);fm(t0,t1,t1);fm(t1,t0,z2);
  fm(t0,t1,t1);for(int i=1;i<5;i++)fm(t0,t0,t0);fm(t1,t0,t1);
  fm(t0,t1,t1);for(int i=1;i<10;i++)fm(t0,t0,t0);fm(t2,t0,t1);
  fm(t0,t2,t2);for(int i=1;i<20;i++)fm(t0,t0,t0);fm(t0,t0,t2);
  fm(t0,t0,t0);for(int i=1;i<10;i++)fm(t0,t0,t0);fm(t1,t0,t1);
  fm(t0,t1,t1);for(int i=1;i<50;i++)fm(t0,t0,t0);fm(t3,t0,t1);
  fm(t0,t3,t3);for(int i=1;i<100;i++)fm(t0,t0,t0);fm(t0,t0,t3);
  fm(t0,t0,t0);for(int i=1;i<50;i++)fm(t0,t0,t0);fm(t0,t0,t1);
  for(int i=0;i<5;i++)fm(t0,t0,t0);fm(t0,t0,t1);
  fm(t0,x2,t0); ft(pub,t0);
}

// ── File-scope state (avoids namespace static visibility issues on GCC 8.4) ───
static PeerInfo  _en_peers[MAX_PEERS];
static int       _en_peer_n  = 0;
static uint32_t  _en_msg_ctr = 0;
static uint32_t  _en_last_hb = 0;
static uint8_t   _en_priv[X25519_LEN];
static uint8_t   _en_pub[X25519_LEN];
static char      _en_name[DEVICE_NAME_LEN];
static void    (*_en_rx_cb)(const PagerMessage&) = nullptr;

// ── Internal helpers ──────────────────────────────────────────────────────────
static bool _en_macEq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static void _en_upsert(const uint8_t* mac, const char* name,
                       const uint8_t* pubkey = nullptr) {
  for (int i = 0; i < _en_peer_n; i++) {
    if (_en_macEq(_en_peers[i].mac, mac)) {
      _en_peers[i].last_seen_ms = millis();
      _en_peers[i].active = true;
      if (pubkey && (!_en_peers[i].has_pubkey ||
          memcmp(_en_peers[i].pubkey, pubkey, X25519_LEN) != 0)) {
        memcpy(_en_peers[i].pubkey, pubkey, X25519_LEN);
        _en_peers[i].has_pubkey = true;
      }
      return;
    }
  }
  if (_en_peer_n >= MAX_PEERS) return;
  PeerInfo* p = &_en_peers[_en_peer_n++];
  memset(p, 0, sizeof(PeerInfo));
  memcpy(p->mac, mac, 6);
  strncpy(p->name, name, DEVICE_NAME_LEN - 1);
  p->last_seen_ms = millis();
  p->active = true;
  if (pubkey) { memcpy(p->pubkey, pubkey, X25519_LEN); p->has_pubkey = true; }
  esp_now_peer_info_t pi{};
  memcpy(pi.peer_addr, mac, 6);
  pi.channel = MESH_CHANNEL;
  pi.encrypt = true;
  memcpy(pi.lmk, _EN_LMK, 16);
  if (!esp_now_is_peer_exist(mac)) esp_now_add_peer(&pi);
  Serial.printf("[ESPNOW] New peer: %s\n", p->name);
}

static void _en_onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len < 1) return;
  uint8_t my_mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, my_mac);
  switch ((PacketType)data[0]) {
    case PKT_HEARTBEAT: {
      if (len < (int)sizeof(HbPacket)) return;
      const HbPacket* hb = (const HbPacket*)data;
      if (_en_macEq(hb->mac, my_mac)) return;
      _en_upsert(hb->mac, hb->name, hb->pubkey);
      break;
    }
    case PKT_MESSAGE: {
      if (len < (int)sizeof(MsgPacket)) return;
      const MsgPacket* pkt = (const MsgPacket*)data;
      if (_en_macEq(pkt->mac, my_mac)) return;
      if (pkt->body[0] == '\0' || (uint8_t)pkt->body[0] < 0x20) return;
      char safe[MAX_MSG_LEN + 1];
      memcpy(safe, pkt->body, MAX_MSG_LEN);
      safe[MAX_MSG_LEN] = '\0';
      _en_upsert(pkt->mac, pkt->name);
      PagerMessage msg{};
      msg.id = pkt->msg_id;
      memcpy(msg.sender_mac, pkt->mac, 6);
      strncpy(msg.sender_name, pkt->name, DEVICE_NAME_LEN - 1);
      strncpy(msg.body, safe, MAX_MSG_LEN - 1);
      msg.is_mine = false;
      if (_en_rx_cb) _en_rx_cb(msg);
      AckPacket ack{};
      ack.type = PKT_ACK;
      esp_wifi_get_mac(WIFI_IF_STA, ack.mac);
      strncpy(ack.name, _en_name, DEVICE_NAME_LEN - 1);
      ack.msg_id = pkt->msg_id;
      esp_now_send(pkt->mac, (uint8_t*)&ack, sizeof(ack));
      Serial.printf("[ESPNOW] MSG from %s: %s\n", pkt->name, safe);
      break;
    }
    case PKT_ACK: {
      if (len < (int)sizeof(AckPacket)) return;
      const AckPacket* ack = (const AckPacket*)data;
      Serial.printf("[ESPNOW] ACK from %s\n", ack->name);
      break;
    }
  }
}

static void _en_onSent(const uint8_t* mac, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) Serial.println("[ESPNOW] Send failed");
}

// =============================================================================
namespace ESPNowManager {

  inline void begin(void (*rx_cb)(const PagerMessage&)) {
    _en_rx_cb = rx_cb;

    // Activate WiFi radio in STA mode — no router, no scan, no connect
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(MESH_CHANNEL, WIFI_SECOND_CHAN_NONE);

    Serial.print("[ESPNOW] MAC: ");
    Serial.println(WiFi.macAddress());

    // Load or generate device name
    {
      Preferences p; p.begin("pgr_id", false);
      String saved = p.getString("name", "");
      if (saved.length() > 0 && saved.length() < DEVICE_NAME_LEN) {
        strncpy(_en_name, saved.c_str(), DEVICE_NAME_LEN - 1);
        Serial.printf("[ID] Name: %s\n", _en_name);
      } else {
        uint8_t mac[6]; esp_wifi_get_mac(WIFI_IF_STA, mac);
        snprintf(_en_name, DEVICE_NAME_LEN, "ND-%02X%02X%02X",
                 mac[3], mac[4], mac[5]);
        p.putString("name", _en_name);
        Serial.printf("[ID] Generated name: %s\n", _en_name);
      }
      p.end();
    }

    // Load or generate X25519 keypair
    {
      Preferences p; p.begin("pgr_keys", false);
      size_t len = p.getBytesLength("priv");
      bool loaded = (len == X25519_LEN &&
                     p.getBytes("priv", _en_priv, X25519_LEN) == X25519_LEN);
      if (!loaded) {
        esp_fill_random(_en_priv, X25519_LEN);
        _en_priv[0] &= 248; _en_priv[31] &= 127; _en_priv[31] |= 64;
        p.putBytes("priv", _en_priv, X25519_LEN);
        Serial.println("[CRYPTO] New X25519 key generated");
      } else {
        Serial.println("[CRYPTO] X25519 key loaded from NVS");
      }
      p.end();
      _en_x25519(_en_pub, _en_priv);
      char hex[X25519_LEN * 2 + 1];
      for (int i = 0; i < X25519_LEN; i++) snprintf(hex+i*2,3,"%02X",_en_pub[i]);
      Serial.printf("[CRYPTO] Public key: %s\n", hex);
    }

    if (esp_now_init() != ESP_OK) {
      Serial.println("[ESPNOW] Init failed!"); return;
    }
    esp_now_set_pmk(_EN_PMK);
    esp_now_register_recv_cb(_en_onRecv);
    esp_now_register_send_cb(_en_onSent);

    esp_now_peer_info_t bcast{};
    memcpy(bcast.peer_addr, BROADCAST_MAC, 6);
    bcast.channel = MESH_CHANNEL;
    bcast.encrypt = false;
    if (!esp_now_is_peer_exist(BROADCAST_MAC)) esp_now_add_peer(&bcast);

    Serial.printf("[ESPNOW] Ready — name:%s  ch:%d\n", _en_name, MESH_CHANNEL);
  }

  inline bool sendMessage(const char* text) {
    if (!text || text[0] == '\0') return false;
    MsgPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PKT_MESSAGE;
    esp_wifi_get_mac(WIFI_IF_STA, pkt.mac);
    strncpy(pkt.name, _en_name, DEVICE_NAME_LEN - 1);
    pkt.msg_id = ++_en_msg_ctr;
    strncpy(pkt.body, text, MAX_MSG_LEN - 1);
    esp_err_t r = esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
    if (r == ESP_OK && _en_rx_cb) {
      PagerMessage sent{};
      sent.id = pkt.msg_id;
      esp_wifi_get_mac(WIFI_IF_STA, sent.sender_mac);
      strncpy(sent.sender_name, _en_name, DEVICE_NAME_LEN - 1);
      strncpy(sent.body, text, MAX_MSG_LEN - 1);
      sent.is_mine = true;
      _en_rx_cb(sent);
    }
    return r == ESP_OK;
  }

  inline void setName(const char* name) {
    strncpy(_en_name, name, DEVICE_NAME_LEN - 1);
    _en_name[DEVICE_NAME_LEN - 1] = '\0';
    Preferences p; p.begin("pgr_id", false);
    p.putString("name", _en_name);
    p.end();
    Serial.printf("[ID] Renamed to: %s\n", _en_name);
  }

  inline const char* myName() { return _en_name; }

  inline void update() {
    uint32_t now = millis();
    if (now - _en_last_hb > HEARTBEAT_MS) {
      _en_last_hb = now;
      HbPacket hb{};
      hb.type = PKT_HEARTBEAT;
      esp_wifi_get_mac(WIFI_IF_STA, hb.mac);
      strncpy(hb.name, _en_name, DEVICE_NAME_LEN - 1);
      memcpy(hb.pubkey, _en_pub, X25519_LEN);
      esp_now_send(BROADCAST_MAC, (uint8_t*)&hb, sizeof(hb));
    }
    for (int i = 0; i < _en_peer_n; i++) {
      if (_en_peers[i].active &&
          (now - _en_peers[i].last_seen_ms > PEER_TIMEOUT_MS)) {
        _en_peers[i].active = false;
        Serial.printf("[ESPNOW] Peer timed out: %s\n", _en_peers[i].name);
      }
    }
  }

  inline int       peerCount()      { return _en_peer_n; }
  inline PeerInfo* getPeer(int i)   { return (i < _en_peer_n) ? &_en_peers[i] : nullptr; }
  inline int activePeerCount() {
    int n = 0;
    for (int i = 0; i < _en_peer_n; i++) if (_en_peers[i].active) n++;
    return n;
  }
}

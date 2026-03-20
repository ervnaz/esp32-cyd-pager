// =============================================================================
//  espnow_manager.h  —  Standalone ESP-NOW mesh (no WiFi router required)
//
//  STARTUP
//  ═══════
//  WiFi is put into STA mode with no SSID — purely to activate the radio.
//  ESP-NOW is initialised on MESH_CHANNEL with AES-128 PMK encryption.
//  No scan, no connect, no DHCP, no internet.
//
//  DEVICE NAME
//  ═══════════
//  On first boot: generated from last 3 MAC bytes → e.g. "ND-A3F2C1"
//  Stored in NVS namespace "pgr_id", key "name".
//  Can be changed on-device via the PEERS screen rename option.
//  Call ESPNowManager::setName(newName) to update and persist.
//
//  ENCRYPTION
//  ══════════
//  Layer 1: ESP-NOW AES-128 PMK  — shared key, all units must match
//  Layer 2: X25519 identity keypair — generated on first boot, stored in NVS
//           Public key broadcast in heartbeats for future direct messaging
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
static const uint8_t _PMK[16] = { PAGER_PMK_BYTES };
static const uint8_t _LMK[16] = { PAGER_LMK_BYTES };

// ── Minimal X25519 public key derivation (no mbedTLS) ────────────────────────
// Only basepoint multiplication needed — private→public key
static void _x25519_base(uint8_t pub[32], const uint8_t priv[32]) {
  typedef int64_t fe[10];
  static const uint8_t BASE[32] = {9};

  auto fecopy  = [](fe o,const fe i){for(int j=0;j<10;j++)o[j]=i[j];};
  auto fecswap = [](fe f,fe g,uint32_t b){
    b=(uint32_t)(-(int32_t)b);
    for(int i=0;i<10;i++){int64_t x=(f[i]^g[i])&(int64_t)b;f[i]^=x;g[i]^=x;}
  };
  auto feadd = [](fe o,const fe a,const fe b){for(int i=0;i<10;i++)o[i]=a[i]+b[i];};
  auto fesub = [](fe o,const fe a,const fe b){for(int i=0;i<10;i++)o[i]=a[i]-b[i];};
  auto fefrombytes = [](fe h,const uint8_t* s){
    h[0]=(int64_t)(s[0]|((uint32_t)s[1]<<8)|((uint32_t)s[2]<<16)|((uint32_t)(s[3]&0x0f)<<24));
    h[1]=(int64_t)((s[3]>>4)|((uint32_t)s[4]<<4)|((uint32_t)s[5]<<12)|((uint32_t)s[6]<<20));
    h[2]=(int64_t)(s[7]|((uint32_t)s[8]<<8)|((uint32_t)s[9]<<16)|((uint32_t)(s[10]&0x0f)<<24));
    h[3]=(int64_t)((s[10]>>4)|((uint32_t)s[11]<<4)|((uint32_t)s[12]<<12)|((uint32_t)s[13]<<20));
    h[4]=(int64_t)(s[14]|((uint32_t)s[15]<<8)|((uint32_t)s[16]<<16)|((uint32_t)(s[17]&0x0f)<<24));
    h[5]=(int64_t)((s[17]>>4)|((uint32_t)s[18]<<4)|((uint32_t)s[19]<<12)|((uint32_t)s[20]<<20));
    h[6]=(int64_t)(s[21]|((uint32_t)s[22]<<8)|((uint32_t)s[23]<<16)|((uint32_t)(s[24]&0x0f)<<24));
    h[7]=(int64_t)((s[24]>>4)|((uint32_t)s[25]<<4)|((uint32_t)s[26]<<12)|((uint32_t)s[27]<<20));
    h[8]=(int64_t)(s[28]|((uint32_t)s[29]<<8)|((uint32_t)s[30]<<16)|((uint32_t)(s[31]&0x0f)<<24));
    h[9]=(int64_t)((s[31]>>4)&0x0fffffff);
  };
  auto femul = [](fe h,const fe f,const fe g){
    int64_t f0=f[0],f1=f[1],f2=f[2],f3=f[3],f4=f[4],f5=f[5],f6=f[6],f7=f[7],f8=f[8],f9=f[9];
    int64_t g0=g[0],g1=g[1],g2=g[2],g3=g[3],g4=g[4],g5=g[5],g6=g[6],g7=g[7],g8=g[8],g9=g[9];
    int64_t g1_19=19*g1,g2_19=19*g2,g3_19=19*g3,g4_19=19*g4,g5_19=19*g5,
            g6_19=19*g6,g7_19=19*g7,g8_19=19*g8,g9_19=19*g9;
    h[0]=f0*g0+f1*g9_19+f2*g8_19+f3*g7_19+f4*g6_19+f5*g5_19+f6*g4_19+f7*g3_19+f8*g2_19+f9*g1_19;
    h[1]=f0*g1+f1*g0+f2*g9_19+f3*g8_19+f4*g7_19+f5*g6_19+f6*g5_19+f7*g4_19+f8*g3_19+f9*g2_19;
    h[2]=f0*g2+f1*g1*2+f2*g0+f3*g9_19+f4*g8_19+f5*g7_19+f6*g6_19+f7*g5_19+f8*g4_19+f9*g3_19;
    h[3]=f0*g3+f1*g2+f2*g1+f3*g0+f4*g9_19+f5*g8_19+f6*g7_19+f7*g6_19+f8*g5_19+f9*g4_19;
    h[4]=f0*g4+f1*g3*2+f2*g2+f3*g1*2+f4*g0+f5*g9_19+f6*g8_19+f7*g7_19+f8*g6_19+f9*g5_19;
    h[5]=f0*g5+f1*g4+f2*g3+f3*g2+f4*g1+f5*g0+f6*g9_19+f7*g8_19+f8*g7_19+f9*g6_19;
    h[6]=f0*g6+f1*g5*2+f2*g4+f3*g3*2+f4*g2+f5*g1*2+f6*g0+f7*g9_19+f8*g8_19+f9*g7_19;
    h[7]=f0*g7+f1*g6+f2*g5+f3*g4+f4*g3+f5*g2+f6*g1+f7*g0+f8*g9_19+f9*g8_19;
    h[8]=f0*g8+f1*g7*2+f2*g6+f3*g5*2+f4*g4+f5*g3*2+f6*g2+f7*g1*2+f8*g0+f9*g9_19;
    h[9]=f0*g9+f1*g8+f2*g7+f3*g6+f4*g5+f5*g4+f6*g3+f7*g2+f8*g1+f9*g0;
    int64_t c0,c1,c2,c3,c4,c5,c6,c7,c8,c9;
    c0=h[0]>>26;h[1]+=c0;h[0]-=c0<<26; c1=h[1]>>25;h[2]+=c1;h[1]-=c1<<25;
    c2=h[2]>>26;h[3]+=c2;h[2]-=c2<<26; c3=h[3]>>25;h[4]+=c3;h[3]-=c3<<25;
    c4=h[4]>>26;h[5]+=c4;h[4]-=c4<<26; c5=h[5]>>25;h[6]+=c5;h[5]-=c5<<25;
    c6=h[6]>>26;h[7]+=c6;h[6]-=c6<<26; c7=h[7]>>25;h[8]+=c7;h[7]-=c7<<25;
    c8=h[8]>>26;h[9]+=c8;h[8]-=c8<<26; c9=h[9]>>25;h[0]+=c9*19;h[9]-=c9<<25;
    c0=h[0]>>26;h[1]+=c0;h[0]-=c0<<26;
  };
  auto fetobytes = [](uint8_t* s,const fe h){
    int64_t h0=h[0],h1=h[1],h2=h[2],h3=h[3],h4=h[4],h5=h[5],h6=h[6],h7=h[7],h8=h[8],h9=h[9];
    int64_t q=(19*h9+(1<<24))>>25;
    q=(h0+q)>>26;q=(h1+q)>>25;q=(h2+q)>>26;q=(h3+q)>>25;
    q=(h4+q)>>26;q=(h5+q)>>25;q=(h6+q)>>26;q=(h7+q)>>25;
    q=(h8+q)>>26;q=(h9+q)>>25;
    h0+=19*q;
    int64_t c=h0>>26;h1+=c;h0-=c<<26; c=h1>>25;h2+=c;h1-=c<<25;
    c=h2>>26;h3+=c;h2-=c<<26; c=h3>>25;h4+=c;h3-=c<<25;
    c=h4>>26;h5+=c;h4-=c<<26; c=h5>>25;h6+=c;h5-=c<<25;
    c=h6>>26;h7+=c;h6-=c<<26; c=h7>>25;h8+=c;h7-=c<<25;
    c=h8>>26;h9+=c;h8-=c<<26;
    s[0]=(uint8_t)(h0);s[1]=(uint8_t)(h0>>8);s[2]=(uint8_t)(h0>>16);
    s[3]=(uint8_t)((h0>>24)|(h1<<2));s[4]=(uint8_t)(h1>>6);s[5]=(uint8_t)(h1>>14);
    s[6]=(uint8_t)((h1>>22)|(h2<<3));s[7]=(uint8_t)(h2>>5);s[8]=(uint8_t)(h2>>13);
    s[9]=(uint8_t)((h2>>21)|(h3<<5));s[10]=(uint8_t)(h3>>3);s[11]=(uint8_t)(h3>>11);
    s[12]=(uint8_t)((h3>>19)|(h4<<6));s[13]=(uint8_t)(h4>>2);s[14]=(uint8_t)(h4>>10);
    s[15]=(uint8_t)(h4>>18);
    s[16]=(uint8_t)(h5);s[17]=(uint8_t)(h5>>8);s[18]=(uint8_t)(h5>>16);
    s[19]=(uint8_t)((h5>>24)|(h6<<1));s[20]=(uint8_t)(h6>>7);s[21]=(uint8_t)(h6>>15);
    s[22]=(uint8_t)((h6>>23)|(h7<<3));s[23]=(uint8_t)(h7>>5);s[24]=(uint8_t)(h7>>13);
    s[25]=(uint8_t)((h7>>21)|(h8<<4));s[26]=(uint8_t)(h8>>4);s[27]=(uint8_t)(h8>>12);
    s[28]=(uint8_t)((h8>>20)|(h9<<6));s[29]=(uint8_t)(h9>>2);s[30]=(uint8_t)(h9>>10);
    s[31]=(uint8_t)(h9>>18);
  };

  uint8_t e[32]; memcpy(e,priv,32);
  e[0]&=248; e[31]&=127; e[31]|=64;
  fe x1,x2,z2,x3,z3;
  fefrombytes(x1,BASE);
  fe one={1}; fecopy(x2,one); fe zero={0}; fecopy(z2,zero);
  fecopy(x3,x1); fecopy(z3,one);
  uint32_t swap=0;
  for(int pos=254;pos>=0;pos--){
    uint32_t b=(e[pos/8]>>(pos&7))&1;
    swap^=b; fecswap(x2,x3,swap); fecswap(z2,z3,swap); swap=b;
    fe A,AA,B,BB,E,C,D,DA,CB;
    feadd(A,x2,z2); femul(AA,A,A); fesub(B,x2,z2); femul(BB,B,B);
    fesub(E,AA,BB); feadd(C,x3,z3); fesub(D,x3,z3);
    femul(DA,D,A); femul(CB,C,B);
    feadd(x3,DA,CB); femul(x3,x3,x3);
    fesub(z3,DA,CB); femul(z3,z3,z3); femul(z3,z3,x1);
    femul(x2,AA,BB);
    fe a24={121665},tmp; femul(tmp,E,a24); feadd(tmp,tmp,AA); femul(z2,E,tmp);
  }
  fecswap(x2,x3,swap); fecswap(z2,z3,swap);
  // Invert z2 via Fermat: z^(p-2)
  fe t0,t1,t2,t3;
  femul(t0,z2,z2); femul(t1,t0,z2); femul(t0,t1,t1); femul(t1,t0,z2);
  femul(t0,t1,t1); for(int i=1;i<5;i++)femul(t0,t0,t0); femul(t1,t0,t1);
  femul(t0,t1,t1); for(int i=1;i<10;i++)femul(t0,t0,t0); femul(t2,t0,t1);
  femul(t0,t2,t2); for(int i=1;i<20;i++)femul(t0,t0,t0); femul(t0,t0,t2);
  femul(t0,t0,t0); for(int i=1;i<10;i++)femul(t0,t0,t0); femul(t1,t0,t1);
  femul(t0,t1,t1); for(int i=1;i<50;i++)femul(t0,t0,t0); femul(t3,t0,t1);
  femul(t0,t3,t3); for(int i=1;i<100;i++)femul(t0,t0,t0); femul(t0,t0,t3);
  femul(t0,t0,t0); for(int i=1;i<50;i++)femul(t0,t0,t0); femul(t0,t0,t1);
  for(int i=0;i<5;i++)femul(t0,t0,t0); femul(t0,t0,t1);
  femul(t0,x2,t0);
  fetobytes(pub,t0);
}

// =============================================================================
namespace ESPNowManager {

  static PeerInfo  _peers[MAX_PEERS];
  static int       _peer_n   = 0;
  static uint32_t  _msg_ctr  = 0;
  static uint32_t  _last_hb  = 0;

  static uint8_t   _my_priv[X25519_LEN];
  static uint8_t   _my_pub[X25519_LEN];
  static char      _my_name[DEVICE_NAME_LEN];

  static void (*_rx_cb)(const PagerMessage&) = nullptr;

  // ── Name: load from NVS, or generate from MAC ──────────────────────────────
  static void _initName() {
    Preferences p;
    p.begin("pgr_id", false);
    String saved = p.getString("name", "");
    if (saved.length() > 0 && saved.length() < DEVICE_NAME_LEN) {
      strncpy(_my_name, saved.c_str(), DEVICE_NAME_LEN - 1);
      Serial.printf("[ID] Loaded name from NVS: %s\n", _my_name);
    } else {
      // Generate from last 3 MAC bytes → "ND-AABBCC"
      uint8_t mac[6];
      esp_wifi_get_mac(WIFI_IF_STA, mac);
      snprintf(_my_name, DEVICE_NAME_LEN, "ND-%02X%02X%02X",
               mac[3], mac[4], mac[5]);
      p.putString("name", _my_name);
      Serial.printf("[ID] Generated name from MAC: %s\n", _my_name);
    }
    p.end();
  }

  // ── Keypair: load or generate via hardware RNG ─────────────────────────────
  static void _initKeypair() {
    Preferences p;
    p.begin("pgr_keys", false);
    size_t len = p.getBytesLength("priv");
    bool loaded = (len == X25519_LEN &&
                   p.getBytes("priv", _my_priv, X25519_LEN) == X25519_LEN);
    if (!loaded) {
      esp_fill_random(_my_priv, X25519_LEN);
      _my_priv[0]  &= 248;
      _my_priv[31] &= 127;
      _my_priv[31] |= 64;
      p.putBytes("priv", _my_priv, X25519_LEN);
      Serial.println("[CRYPTO] New X25519 private key generated");
    } else {
      Serial.println("[CRYPTO] X25519 private key loaded from NVS");
    }
    p.end();
    _x25519_base(_my_pub, _my_priv);
    char hex[X25519_LEN * 2 + 1];
    for (int i = 0; i < X25519_LEN; i++) snprintf(hex + i*2, 3, "%02X", _my_pub[i]);
    Serial.printf("[CRYPTO] Public key: %s\n", hex);
  }

  // ── Helpers ────────────────────────────────────────────────────────────────
  static bool _macEq(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, 6) == 0;
  }

  static void _upsertPeer(const uint8_t* mac, const char* name,
                          const uint8_t* pubkey = nullptr) {
    // Update existing
    for (int i = 0; i < _peer_n; i++) {
      if (_macEq(_peers[i].mac, mac)) {
        _peers[i].last_seen_ms = millis();
        _peers[i].active = true;
        if (pubkey && (!_peers[i].has_pubkey ||
            memcmp(_peers[i].pubkey, pubkey, X25519_LEN) != 0)) {
          memcpy(_peers[i].pubkey, pubkey, X25519_LEN);
          _peers[i].has_pubkey = true;
        }
        return;
      }
    }
    // New peer
    if (_peer_n >= MAX_PEERS) return;
    PeerInfo* p = &_peers[_peer_n++];
    memset(p, 0, sizeof(PeerInfo));
    memcpy(p->mac, mac, 6);
    strncpy(p->name, name, DEVICE_NAME_LEN - 1);
    p->last_seen_ms = millis();
    p->active = true;
    if (pubkey) { memcpy(p->pubkey, pubkey, X25519_LEN); p->has_pubkey = true; }

    // Register with ESP-NOW (LMK encrypted unicast)
    esp_now_peer_info_t pi{};
    memcpy(pi.peer_addr, mac, 6);
    pi.channel = MESH_CHANNEL;
    pi.encrypt = true;
    memcpy(pi.lmk, _LMK, 16);
    if (!esp_now_is_peer_exist(mac)) esp_now_add_peer(&pi);
    Serial.printf("[ESPNOW] New peer: %s\n", p->name);
  }

  // ── Receive callback ───────────────────────────────────────────────────────
  static void _onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < 1) return;
    uint8_t my_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, my_mac);

    switch ((PacketType)data[0]) {
      case PKT_HEARTBEAT: {
        if (len < (int)sizeof(HbPacket)) return;
        const HbPacket* hb = (const HbPacket*)data;
        if (_macEq(hb->mac, my_mac)) return;
        _upsertPeer(hb->mac, hb->name, hb->pubkey);
        break;
      }
      case PKT_MESSAGE: {
        if (len < (int)sizeof(MsgPacket)) return;
        const MsgPacket* pkt = (const MsgPacket*)data;
        if (_macEq(pkt->mac, my_mac)) return;
        if (pkt->body[0] == '\0') return;
        if ((uint8_t)pkt->body[0] < 0x20) return;
        _upsertPeer(pkt->mac, pkt->name);

        // Safe copy
        char safe[MAX_MSG_LEN + 1];
        memcpy(safe, pkt->body, MAX_MSG_LEN);
        safe[MAX_MSG_LEN] = '\0';

        PagerMessage msg{};
        msg.id = pkt->msg_id;
        memcpy(msg.sender_mac, pkt->mac, 6);
        strncpy(msg.sender_name, pkt->name, DEVICE_NAME_LEN - 1);
        strncpy(msg.body, safe, MAX_MSG_LEN - 1);
        msg.is_mine = false;
        if (_rx_cb) _rx_cb(msg);

        // ACK
        AckPacket ack{};
        ack.type = PKT_ACK;
        esp_wifi_get_mac(WIFI_IF_STA, ack.mac);
        strncpy(ack.name, _my_name, DEVICE_NAME_LEN - 1);
        ack.msg_id = pkt->msg_id;
        esp_now_send(pkt->mac, (uint8_t*)&ack, sizeof(ack));
        Serial.printf("[ESPNOW] MSG from %s: %s\n", pkt->name, safe);
        break;
      }
      case PKT_ACK: {
        if (len < (int)sizeof(AckPacket)) return;
        const AckPacket* ack = (const AckPacket*)data;
        Serial.printf("[ESPNOW] ACK from %s for msg %u\n", ack->name, ack->msg_id);
        break;
      }
    }
  }

  static void _onSent(const uint8_t* mac, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS)
      Serial.println("[ESPNOW] Send failed");
  }

  // ── Public API ─────────────────────────────────────────────────────────────
  inline void begin(void (*rx_cb)(const PagerMessage&)) {
    _rx_cb = rx_cb;

    // WiFi radio on — STA mode, no router, no scan, no connect
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    // Lock to fixed channel — no router needed
    esp_wifi_set_channel(MESH_CHANNEL, WIFI_SECOND_CHAN_NONE);

    Serial.print("[ESPNOW] MAC: ");
    Serial.println(WiFi.macAddress());

    _initName();
    _initKeypair();

    if (esp_now_init() != ESP_OK) {
      Serial.println("[ESPNOW] Init failed!");
      return;
    }
    esp_now_set_pmk(_PMK);
    esp_now_register_recv_cb(_onRecv);
    esp_now_register_send_cb(_onSent);

    // Broadcast peer (unencrypted — required for broadcast address)
    esp_now_peer_info_t bcast{};
    memcpy(bcast.peer_addr, BROADCAST_MAC, 6);
    bcast.channel = MESH_CHANNEL;
    bcast.encrypt = false;
    if (!esp_now_is_peer_exist(BROADCAST_MAC)) esp_now_add_peer(&bcast);

    Serial.printf("[ESPNOW] Ready — name:%s  ch:%d  AES-128 PMK active\n",
                  _my_name, MESH_CHANNEL);
  }

  inline bool sendMessage(const char* text) {
    if (!text || text[0] == '\0') return false;
    MsgPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PKT_MESSAGE;
    esp_wifi_get_mac(WIFI_IF_STA, pkt.mac);
    strncpy(pkt.name, _my_name, DEVICE_NAME_LEN - 1);
    pkt.msg_id = ++_msg_ctr;
    strncpy(pkt.body, text, MAX_MSG_LEN - 1);
    pkt.body[MAX_MSG_LEN - 1] = '\0';
    esp_err_t r = esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));

    // Add sent message to local store via callback
    if (r == ESP_OK && _rx_cb) {
      PagerMessage sent{};
      sent.id = pkt.msg_id;
      esp_wifi_get_mac(WIFI_IF_STA, sent.sender_mac);
      strncpy(sent.sender_name, _my_name, DEVICE_NAME_LEN - 1);
      strncpy(sent.body, text, MAX_MSG_LEN - 1);
      sent.is_mine = true;
      _rx_cb(sent);
    }
    return r == ESP_OK;
  }

  // Rename this device — persists to NVS, takes effect immediately
  inline void setName(const char* name) {
    strncpy(_my_name, name, DEVICE_NAME_LEN - 1);
    _my_name[DEVICE_NAME_LEN - 1] = '\0';
    Preferences p;
    p.begin("pgr_id", false);
    p.putString("name", _my_name);
    p.end();
    Serial.printf("[ID] Name changed to: %s\n", _my_name);
  }

  inline const char* myName()   { return _my_name; }

  inline void update() {
    uint32_t now = millis();
    if (now - _last_hb > HEARTBEAT_MS) {
      _last_hb = now;
      HbPacket hb{};
      hb.type = PKT_HEARTBEAT;
      esp_wifi_get_mac(WIFI_IF_STA, hb.mac);
      strncpy(hb.name, _my_name, DEVICE_NAME_LEN - 1);
      memcpy(hb.pubkey, _my_pub, X25519_LEN);
      esp_now_send(BROADCAST_MAC, (uint8_t*)&hb, sizeof(hb));
    }
    for (int i = 0; i < _peer_n; i++) {
      if (_peers[i].active &&
          (now - _peers[i].last_seen_ms > PEER_TIMEOUT_MS)) {
        _peers[i].active = false;
        Serial.printf("[ESPNOW] Peer timed out: %s\n", _peers[i].name);
      }
    }
  }

  inline int       peerCount()         { return _peer_n; }
  inline PeerInfo* getPeer(int i)       { return (i<_peer_n)?&_peers[i]:nullptr; }
  inline int       activePeerCount()    {
    int n = 0;
    for (int i = 0; i < _peer_n; i++) if (_peers[i].active) n++;
    return n;
  }
}

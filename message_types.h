// =============================================================================
//  message_types.h  —  Wire format and application data structures
// =============================================================================
#pragma once
#include <stdint.h>
#include "config.h"

// ── Packet types ─────────────────────────────────────────────────────────────
enum PacketType : uint8_t {
  PKT_HEARTBEAT = 0x01,   // identity + X25519 pubkey
  PKT_MESSAGE   = 0x02,   // broadcast text message
  PKT_ACK       = 0x03,   // delivery acknowledgement
};

// ── Heartbeat — broadcast every HEARTBEAT_MS ─────────────────────────────────
#define X25519_LEN 32
struct __attribute__((packed)) HbPacket {
  PacketType type;
  uint8_t    mac[6];
  char       name[DEVICE_NAME_LEN];
  uint8_t    pubkey[X25519_LEN];
};  // 48 bytes

// ── Message ───────────────────────────────────────────────────────────────────
struct __attribute__((packed)) MsgPacket {
  PacketType type;
  uint8_t    mac[6];
  char       name[DEVICE_NAME_LEN];
  uint32_t   msg_id;
  char       body[MAX_MSG_LEN];
};  // 220 bytes ✓  (< 250 byte ESP-NOW limit)

// ── ACK ───────────────────────────────────────────────────────────────────────
struct __attribute__((packed)) AckPacket {
  PacketType type;
  uint8_t    mac[6];
  char       name[DEVICE_NAME_LEN];
  uint32_t   msg_id;
};  // 20 bytes

// ── Application message (stored in MessageStore) ─────────────────────────────
struct PagerMessage {
  uint32_t id;
  char     sender_name[DEVICE_NAME_LEN];
  uint8_t  sender_mac[6];
  char     body[MAX_MSG_LEN];
  bool     is_mine;
  bool     ack_received;
};

// ── Peer ──────────────────────────────────────────────────────────────────────
struct PeerInfo {
  uint8_t  mac[6];
  char     name[DEVICE_NAME_LEN];
  uint32_t last_seen_ms;
  bool     active;
  bool     has_pubkey;
  uint8_t  pubkey[X25519_LEN];
};

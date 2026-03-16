// =============================================================================
//  message_types.h  —  Shared data structures transmitted over ESP-NOW
// =============================================================================
#pragma once
#include <stdint.h>
#include "config.h"

// ── Packet types ─────────────────────────────────────────────────────────────
enum PacketType : uint8_t {
  PKT_MESSAGE   = 0x01,
  PKT_HEARTBEAT = 0x02,
  PKT_ACK       = 0x03,
};

// ── Wire format (must be ≤ 250 bytes for ESP-NOW) ────────────────────────────
struct __attribute__((packed)) EspNowPacket {
  PacketType  type;
  uint8_t     sender_mac[6];
  char        sender_name[DEVICE_NAME_LEN];
  uint32_t    msg_id;
  uint32_t    timestamp_s;     // seconds since boot
  char        body[MAX_MSG_LEN];
};
// sizeof = 1+6+9+4+4+200 = 224 bytes  ✓

// ── Application-level message (stored in MessageStore) ───────────────────────
struct PagerMessage {
  uint32_t  id;
  uint32_t  timestamp_s;
  char      sender_name[DEVICE_NAME_LEN];
  uint8_t   sender_mac[6];
  char      body[MAX_MSG_LEN];
  bool      is_mine;
  bool      ack_received;
};

// ── Peer descriptor ──────────────────────────────────────────────────────────
struct PeerInfo {
  uint8_t  mac[6];
  char     name[DEVICE_NAME_LEN];
  uint32_t last_seen_ms;
  bool     active;
};

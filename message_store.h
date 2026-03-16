// =============================================================================
//  message_store.h  —  Inbox ring-buffer with history
// =============================================================================
#pragma once
#include "message_types.h"
#include "config.h"

namespace MessageStore {

  static PagerMessage _msgs[MAX_MESSAGES];
  static int          _count   = 0;
  static int          _head    = 0;
  static int          _unread  = 0;

  // Add a message (overwrites oldest when full)
  inline void add(const PagerMessage& msg) {
    _msgs[_head] = msg;
    _head = (_head + 1) % MAX_MESSAGES;
    if (_count < MAX_MESSAGES) _count++;
    if (!msg.is_mine) _unread++;
  }

  inline int  count()       { return _count; }
  inline int  unread()      { return _unread; }
  inline void clearUnread() { _unread = 0; }

  // Return message by display index — 0 = newest
  inline const PagerMessage* get(int display_idx) {
    if (display_idx < 0 || display_idx >= _count) return nullptr;
    int ring = (_head - 1 - display_idx + MAX_MESSAGES * 2) % MAX_MESSAGES;
    return &_msgs[ring];
  }

  inline void markAck(uint32_t msg_id) {
    for (int i = 0; i < MAX_MESSAGES; i++) {
      if (_msgs[i].id == msg_id) {
        _msgs[i].ack_received = true;
        break;
      }
    }
  }
}

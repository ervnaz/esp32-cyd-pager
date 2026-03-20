// =============================================================================
//  message_store.h  —  Inbox ring-buffer + NVS persistence (Preferences)
// =============================================================================
#pragma once
#include "message_types.h"
#include "config.h"
#include <Preferences.h>

namespace MessageStore {

  static PagerMessage _msgs[MAX_MESSAGES];
  static int          _count  = 0;
  static int          _head   = 0;
  static int          _unread = 0;
  static Preferences  _prefs;

  // ── NVS save — persists newest MSG_HISTORY received messages ───────────────
  static void _save() {
    _prefs.begin("pgr_inbox", false);
    int saved = 0;
    for (int di = 0; di < _count && saved < MSG_HISTORY; di++) {
      const PagerMessage* m = get(di);
      if (!m) continue;
      char kb[10], kf[10], km[10];
      snprintf(kb, sizeof(kb), "body%d", saved);
      snprintf(kf, sizeof(kf), "from%d", saved);
      snprintf(km, sizeof(km), "mine%d", saved);
      _prefs.putString(kb, m->body);
      _prefs.putString(kf, m->sender_name);
      _prefs.putBool(km, m->is_mine);
      saved++;
    }
    // Clear leftover slots
    for (int i = saved; i < MSG_HISTORY; i++) {
      char kb[10], kf[10], km[10];
      snprintf(kb, sizeof(kb), "body%d", i);
      snprintf(kf, sizeof(kf), "from%d", i);
      snprintf(km, sizeof(km), "mine%d", i);
      _prefs.remove(kb); _prefs.remove(kf); _prefs.remove(km);
    }
    _prefs.end();
  }

  // ── NVS load ───────────────────────────────────────────────────────────────
  inline void load() {
    _prefs.begin("pgr_inbox", true);
    for (int i = 0; i < MSG_HISTORY; i++) {
      char kb[10], kf[10], km[10];
      snprintf(kb, sizeof(kb), "body%d", i);
      snprintf(kf, sizeof(kf), "from%d", i);
      snprintf(km, sizeof(km), "mine%d", i);
      String body = _prefs.getString(kb, "");
      String from = _prefs.getString(kf, "");
      if (body.length() == 0) break;
      // Reject binary garbage
      if ((uint8_t)body[0] < 0x20) continue;
      PagerMessage msg{};
      msg.id = i;
      strncpy(msg.body, body.c_str(), MAX_MSG_LEN - 1);
      strncpy(msg.sender_name, from.c_str(), DEVICE_NAME_LEN - 1);
      msg.is_mine = _prefs.getBool(km, false);
      _msgs[_head] = msg;
      _head = (_head + 1) % MAX_MESSAGES;
      if (_count < MAX_MESSAGES) _count++;
    }
    _prefs.end();
    Serial.printf("[STORE] Loaded %d messages from NVS\n", _count);
  }

  // ── Public API ─────────────────────────────────────────────────────────────
  inline void add(const PagerMessage& msg) {
    _msgs[_head] = msg;
    _head = (_head + 1) % MAX_MESSAGES;
    if (_count < MAX_MESSAGES) _count++;
    if (!msg.is_mine) _unread++;
    _save();
  }

  inline int  count()       { return _count; }
  inline int  unread()      { return _unread; }
  inline void clearUnread() { _unread = 0; }

  // 0 = newest
  inline const PagerMessage* get(int display_idx) {
    if (display_idx < 0 || display_idx >= _count) return nullptr;
    int ring = (_head - 1 - display_idx + MAX_MESSAGES * 2) % MAX_MESSAGES;
    return &_msgs[ring];
  }

  inline void markAck(uint32_t msg_id) {
    for (int i = 0; i < MAX_MESSAGES; i++)
      if (_msgs[i].id == msg_id) { _msgs[i].ack_received = true; break; }
  }

  inline void clearAll() {
    _prefs.begin("pgr_inbox", false);
    _prefs.clear();
    _prefs.end();
    _count = _head = _unread = 0;
    memset(_msgs, 0, sizeof(_msgs));
  }
}

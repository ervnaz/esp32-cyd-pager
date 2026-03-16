// =============================================================================
//  ui.cpp  —  All screen drawing and touch handling
//  Features: message history, timestamps, SHIFT key, unread auto-clear
// =============================================================================
#include "ui.h"
#include "config.h"
#include "message_store.h"
#include "espnow_manager.h"
#include <TFT_eSPI.h>
#include <Arduino.h>

// ── Module state ─────────────────────────────────────────────────────────────
static TFT_eSPI*  _tft            = nullptr;
static ScreenID   _current_screen = SCREEN_INBOX;
static bool       _was_pressed    = false;
static uint32_t   _last_touch_ms  = 0;

// Notification overlay
static bool       _notify_visible  = false;
static uint32_t   _notify_start_ms = 0;
static char       _notify_sender[DEVICE_NAME_LEN] = "";
static char       _notify_preview[48] = "";

// Compose state
static char       _compose_buf[MAX_MSG_LEN] = "";
static int        _compose_len              = 0;
static bool       _shift                    = false;

// ── Colour helpers ────────────────────────────────────────────────────────────
static void fill(int x, int y, int w, int h, uint16_t c) {
  _tft->fillRect(x, y, w, h, c);
}
static void rect(int x, int y, int w, int h, uint16_t c) {
  _tft->drawRect(x, y, w, h, c);
}
static void hline(int x, int y, int w, uint16_t c) {
  _tft->drawFastHLine(x, y, w, c);
}
static void txt(int x, int y, const char* s, uint16_t fg, uint16_t bg = COL_BG, uint8_t sz = 1) {
  _tft->setTextColor(fg, bg);
  _tft->setTextSize(sz);
  _tft->setCursor(x, y);
  _tft->print(s);
}

// ── Status bar ────────────────────────────────────────────────────────────────
static void drawStatusBar() {
  fill(0, 0, SCREEN_W, STATUS_H, COL_PANEL);

  // Device name
  _tft->setTextColor(COL_YELLOW, COL_PANEL);
  _tft->setTextSize(1);
  _tft->setCursor(4, 4);
  _tft->print(DEVICE_NAME);

  // Peer count (centre)
  int active = 0;
  for (int i = 0; i < ESPNowManager::peerCount(); i++)
    if (ESPNowManager::getPeer(i)->active) active++;
  char buf[20];
  snprintf(buf, sizeof(buf), "%d peer%s", active, active == 1 ? "" : "s");
  _tft->setTextColor(COL_LTGRAY, COL_PANEL);
  int tw = strlen(buf) * 6;
  _tft->setCursor((SCREEN_W - tw) / 2, 4);
  _tft->print(buf);

  // Unread badge (right)
  int unread = MessageStore::unread();
  if (unread > 0) {
    snprintf(buf, sizeof(buf), "[%d]", unread);
    _tft->setTextColor(COL_ORANGE, COL_PANEL);
    _tft->setCursor(SCREEN_W - (int)strlen(buf) * 6 - 4, 4);
    _tft->print(buf);
  }
}

// ── Tab bar (bottom) ──────────────────────────────────────────────────────────
static void drawTabBar() {
  const char* labels[3] = {"INBOX", "COMPOSE", "PEERS"};
  int tw = SCREEN_W / 3;
  fill(0, TAB_Y, SCREEN_W, TAB_H, COL_DKBLUE);

  for (int i = 0; i < 3; i++) {
    bool active = (i == (int)_current_screen);
    if (active) fill(i * tw, TAB_Y, tw, TAB_H, COL_ACTIVE);
    _tft->setTextColor(active ? COL_BG : COL_WHITE, active ? COL_ACTIVE : COL_DKBLUE);
    _tft->setTextSize(1);
    int lx = i * tw + (tw - (int)strlen(labels[i]) * 6) / 2;
    _tft->setCursor(lx, TAB_Y + 10);
    _tft->print(labels[i]);
    if (i < 2) _tft->drawFastVLine(i * tw + tw, TAB_Y, TAB_H, COL_BG);
  }
  hline(0, TAB_Y, SCREEN_W, COL_ACTIVE);
}

// ── INBOX screen ──────────────────────────────────────────────────────────────
static void drawInbox() {
  fill(0, CON_Y, SCREEN_W, CON_H, COL_BG);

  int count = MessageStore::count();
  int shown = 0;

  // Show received messages (newest first, skip sent)
  for (int i = 0; i < count && shown < MSG_HISTORY; i++) {
    const PagerMessage* m = MessageStore::get(i);
    if (!m || m->is_mine) continue;

    int my = CON_Y + 4 + shown * 32;
    if (my + 30 > TAB_Y) break;

    bool newest = (shown == 0);
    fill(2, my, SCREEN_W - 4, 30, COL_DKBLUE);
    rect(2, my, SCREEN_W - 4, 30, newest ? COL_ACTIVE : COL_PANEL);

    // Header: sender + relative age
    char hdr[40];
    uint32_t age = millis() / 1000 - m->timestamp_s;
    if (age < 60)        snprintf(hdr, sizeof(hdr), "%s  %us ago",  m->sender_name, age);
    else if (age < 3600) snprintf(hdr, sizeof(hdr), "%s  %um ago",  m->sender_name, age / 60);
    else                 snprintf(hdr, sizeof(hdr), "%s  %uh ago",  m->sender_name, age / 3600);

    _tft->setTextColor(newest ? COL_YELLOW : COL_LTGRAY, COL_DKBLUE);
    _tft->setTextSize(1);
    _tft->setCursor(6, my + 3);
    _tft->print(hdr);

    // Message body (truncated to fit)
    _tft->setTextColor(COL_WHITE, COL_DKBLUE);
    _tft->setCursor(6, my + 16);
    char preview[48] = "";
    strncpy(preview, m->body, 47);
    _tft->print(preview);

    // ACK tick
    if (m->ack_received) {
      _tft->setTextColor(COL_GREEN, COL_DKBLUE);
      _tft->setCursor(SCREEN_W - 14, my + 3);
      _tft->print("*");
    }
    shown++;
  }

  if (shown == 0) {
    _tft->setTextColor(COL_LTGRAY, COL_BG);
    _tft->setTextSize(1);
    _tft->setCursor(6, CON_Y + 40);
    _tft->print("No messages yet");
    _tft->setCursor(6, CON_Y + 56);
    _tft->print("Go to COMPOSE to send one");
  }

  // Last sent message strip at bottom of content area
  const PagerMessage* last_sent = nullptr;
  for (int i = 0; i < count; i++) {
    const PagerMessage* m = MessageStore::get(i);
    if (m && m->is_mine) { last_sent = m; break; }
  }
  if (last_sent) {
    int sy = TAB_Y - 26;
    fill(2, sy, SCREEN_W - 4, 22, COL_PANEL);
    rect(2, sy, SCREEN_W - 4, 22, COL_LTGRAY);
    _tft->setTextColor(COL_YELLOW, COL_PANEL);
    _tft->setCursor(6, sy + 6);
    _tft->print("YOU:");
    _tft->setTextColor(COL_WHITE, COL_PANEL);
    _tft->setCursor(34, sy + 6);
    char preview[48] = "";
    strncpy(preview, last_sent->body, 47);
    _tft->print(preview);
  }
}

// ── COMPOSE screen ────────────────────────────────────────────────────────────
static bool _compose_dirty = true;

static void drawCompose() {
  if (!_compose_dirty) return;
  _compose_dirty = false;

  fill(0, CON_Y, SCREEN_W, CON_H, COL_BG);

  // Text box
  int bx = 2, by = CON_Y + 2, bw = SCREEN_W - 4, bh = 28;
  fill(bx, by, bw, bh, 0x0003);   // very dark blue
  rect(bx, by, bw, bh, COL_ACTIVE);

  _tft->setTextColor(COL_YELLOW, 0x0003);
  _tft->setCursor(6, by + 5);
  _tft->print("MSG:");
  _tft->setTextColor(COL_WHITE, 0x0003);
  _tft->setCursor(36, by + 5);
  _tft->print(_compose_buf);

  // Char count
  char cc[12];
  snprintf(cc, sizeof(cc), "%d/100", _compose_len);
  _tft->setTextColor(COL_LTGRAY, 0x0003);
  _tft->setCursor(SCREEN_W - (int)strlen(cc) * 6 - 4, by + 5);
  _tft->print(cc);

  // Cursor
  int cur_x = 36 + _compose_len * 6;
  if (cur_x < SCREEN_W - 52) {
    _tft->setTextColor(COL_ACTIVE, 0x0003);
    _tft->setCursor(cur_x, by + 16);
    _tft->print("_");
  }

  // ── Keyboard ────────────────────────────────────────────────────────────
  const char* r1 = _shift ? "QWERTYUIOP" : "qwertyuiop";
  const char* r2 = _shift ? "ASDFGHJKL." : "asdfghjkl.";
  const char* r3 = _shift ? "ZXCVBNM,"   : "zxcvbnm,";

  for (int i = 0; i < 10; i++) {
    int kx = KB_KX + i * (KB_KW + KB_KG);

    // Row 1
    fill(kx, KB_KY, KB_KW, KB_KH, COL_DKBLUE);
    rect(kx, KB_KY, KB_KW, KB_KH, COL_ACTIVE);
    char l1[2] = {r1[i], 0};
    _tft->setTextColor(COL_WHITE, COL_DKBLUE);
    _tft->setCursor(kx + 9, KB_KY + 10);
    _tft->print(l1);

    // Row 2
    int ky2 = KB_KY + KB_KH + KB_KG;
    fill(kx, ky2, KB_KW, KB_KH, COL_DKBLUE);
    rect(kx, ky2, KB_KW, KB_KH, COL_ACTIVE);
    char l2[2] = {r2[i], 0};
    _tft->setTextColor(COL_WHITE, COL_DKBLUE);
    _tft->setCursor(kx + 9, ky2 + 10);
    _tft->print(l2);
  }

  // Row 3: Z-M + , + DEL
  int ky3 = KB_KY + 2 * (KB_KH + KB_KG);
  for (int i = 0; i < 8; i++) {
    int kx = KB_KX + i * (KB_KW + KB_KG);
    fill(kx, ky3, KB_KW, KB_KH, COL_DKBLUE);
    rect(kx, ky3, KB_KW, KB_KH, COL_ACTIVE);
    char l3[2] = {r3[i], 0};
    _tft->setTextColor(COL_WHITE, COL_DKBLUE);
    _tft->setCursor(kx + 9, ky3 + 10);
    _tft->print(l3);
  }
  int del_x = KB_KX + 8 * (KB_KW + KB_KG);
  fill(del_x, ky3, KB_KW * 2 + KB_KG, KB_KH, COL_DKRED);
  rect(del_x, ky3, KB_KW * 2 + KB_KG, KB_KH, COL_RED);
  _tft->setTextColor(COL_WHITE, COL_DKRED);
  _tft->setCursor(del_x + 8, ky3 + 10);
  _tft->print("DEL");

  // Row 4: SHF + SPACE + SEND
  int ky4 = KB_KY + 3 * (KB_KH + KB_KG);
  // SHIFT
  fill(KB_KX, ky4, KB_KW * 2 + KB_KG, KB_KH, _shift ? COL_ACTIVE : COL_DKBLUE);
  rect(KB_KX, ky4, KB_KW * 2 + KB_KG, KB_KH, _shift ? COL_YELLOW : COL_ACTIVE);
  _tft->setTextColor(_shift ? COL_BG : COL_WHITE, _shift ? COL_ACTIVE : COL_DKBLUE);
  _tft->setCursor(KB_KX + 8, ky4 + 10);
  _tft->print("SHF");
  // SPACE
  int spc_x = KB_KX + KB_KW * 2 + KB_KG * 2;
  fill(spc_x, ky4, KB_KW * 5 + KB_KG * 4, KB_KH, COL_DKBLUE);
  rect(spc_x, ky4, KB_KW * 5 + KB_KG * 4, KB_KH, COL_ACTIVE);
  _tft->setTextColor(COL_WHITE, COL_DKBLUE);
  _tft->setCursor(spc_x + 50, ky4 + 10);
  _tft->print("SPC");
  // SEND
  int snd_x = KB_KX + KB_KW * 7 + KB_KG * 7;
  fill(snd_x, ky4, KB_KW * 3 + KB_KG * 2, KB_KH, 0x0320);  // dark green
  rect(snd_x, ky4, KB_KW * 3 + KB_KG * 2, KB_KH, COL_GREEN);
  _tft->setTextColor(COL_BG, 0x0320);
  _tft->setCursor(snd_x + 10, ky4 + 10);
  _tft->print("SEND");
}

// ── PEERS screen ──────────────────────────────────────────────────────────────
static void drawPeers() {
  fill(0, CON_Y, SCREEN_W, CON_H, COL_BG);

  _tft->setTextColor(COL_YELLOW, COL_BG);
  _tft->setTextSize(1);
  _tft->setCursor(6, CON_Y + 4);
  _tft->print("Known peers:");
  hline(0, CON_Y + 16, SCREEN_W, COL_DKBLUE);

  int cnt = ESPNowManager::peerCount();

  if (cnt == 0) {
    _tft->setTextColor(COL_LTGRAY, COL_BG);
    _tft->setCursor(6, CON_Y + 30);
    _tft->print("No peers seen yet");
    return;
  }

  int row_h = 22;
  for (int i = 0; i < cnt && i < 7; i++) {
    PeerInfo* p = ESPNowManager::getPeer(i);
    if (!p) continue;
    int py = CON_Y + 20 + i * row_h;
    uint16_t row_col = p->active ? COL_DKBLUE : COL_BG;
    fill(2, py, SCREEN_W - 4, row_h - 2, row_col);

    // Status dot
    _tft->fillRect(6, py + 7, 8, 8, p->active ? COL_GREEN : COL_RED);

    // Name
    _tft->setTextColor(p->active ? COL_WHITE : COL_LTGRAY, row_col);
    _tft->setCursor(20, py + 7);
    _tft->print(p->name);

    // Last seen age
    uint32_t age = (millis() - p->last_seen_ms) / 1000;
    char age_s[12];
    if (age < 60)  snprintf(age_s, sizeof(age_s), "%us", age);
    else           snprintf(age_s, sizeof(age_s), "%um", age / 60);
    _tft->setTextColor(COL_LTGRAY, row_col);
    _tft->setCursor(90, py + 7);
    _tft->print(age_s);

    // Partial MAC
    char mac[14];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X", p->mac[3], p->mac[4], p->mac[5]);
    _tft->setCursor(SCREEN_W - 78, py + 7);
    _tft->print(mac);
  }
}

// ── Notification banner ───────────────────────────────────────────────────────
static void drawNotification() {
  if (!_notify_visible) return;
  if (millis() - _notify_start_ms > NOTIFY_DISPLAY_MS) {
    _notify_visible = false;
    // Redraw content under banner
    if (_current_screen == SCREEN_INBOX)        drawInbox();
    else if (_current_screen == SCREEN_COMPOSE) { _compose_dirty = true; drawCompose(); }
    else                                         drawPeers();
    return;
  }

  int nx = 4, ny = CON_Y + 2, nw = SCREEN_W - 8, nh = 40;
  fill(nx, ny, nw, nh, COL_ORANGE);
  rect(nx, ny, nw, nh, COL_WHITE);
  char hdr[32];
  snprintf(hdr, sizeof(hdr), "MSG FROM: %s", _notify_sender);
  _tft->setTextColor(COL_BG, COL_ORANGE);
  _tft->setTextSize(1);
  _tft->setCursor(nx + 4, ny + 4);
  _tft->print(hdr);
  _tft->setCursor(nx + 4, ny + 18);
  _tft->print(_notify_preview);
}

// ── Touch handlers ────────────────────────────────────────────────────────────
static bool handleTabTouch(int16_t x, int16_t y) {
  if (y < TAB_Y - 4 || y > SCREEN_H) return false;
  int tab = x / (SCREEN_W / 3);
  tab = constrain(tab, 0, 2);
  ScreenID next = (ScreenID)tab;
  if (next == SCREEN_INBOX) MessageStore::clearUnread();  // auto-clear on open
  UI::showScreen(next);
  return true;
}

static void handleInboxTouch(int16_t x, int16_t y) {
  MessageStore::clearUnread();
  drawStatusBar();
  (void)x; (void)y;
}

static void handleComposeTouch(int16_t x, int16_t y) {
  const int KX = KB_KX, KY = KB_KY, KW = KB_KW, KH = KB_KH, KG = KB_KG;

  if (y < KY || y > KY + 4 * (KH + KG)) return;

  int row = (y - KY) / (KH + KG);
  int col = (x - KX) / (KW + KG);
  if (col < 0 || col > 9) return;

  const char* rows_up[3] = {"QWERTYUIOP", "ASDFGHJKL.", "ZXCVBNM,"};
  const char* rows_lo[3] = {"qwertyuiop", "asdfghjkl.", "zxcvbnm,"};
  const char** rows = _shift ? rows_up : rows_lo;

  if (row == 0 && col < 10) {
    if (_compose_len < MAX_MSG_LEN - 1) { _compose_buf[_compose_len++] = rows[0][col]; _compose_buf[_compose_len] = 0; _shift = false; }
  } else if (row == 1 && col < 10) {
    if (_compose_len < MAX_MSG_LEN - 1) { _compose_buf[_compose_len++] = rows[1][col]; _compose_buf[_compose_len] = 0; _shift = false; }
  } else if (row == 2) {
    if (col < 8) {
      if (_compose_len < MAX_MSG_LEN - 1) { _compose_buf[_compose_len++] = rows[2][col]; _compose_buf[_compose_len] = 0; _shift = false; }
    } else {
      // DEL
      if (_compose_len > 0) _compose_buf[--_compose_len] = 0;
    }
  } else if (row == 3) {
    int snd_x = KX + KW * 7 + KG * 7;
    int spc_x = KX + KW * 2 + KG * 2;
    if (x >= snd_x) {
      // SEND
      if (_compose_len > 0) {
        bool ok = ESPNowManager::sendMessage(_compose_buf);
        Serial.printf("[UI] Send %s: %s\n", ok ? "OK" : "FAIL", _compose_buf);
        memset(_compose_buf, 0, sizeof(_compose_buf));
        _compose_len = 0;
        _shift = false;
        if (ok) UI::showScreen(SCREEN_INBOX);
      }
      return;
    } else if (x >= spc_x) {
      if (_compose_len < MAX_MSG_LEN - 1) { _compose_buf[_compose_len++] = ' '; _compose_buf[_compose_len] = 0; }
    } else {
      // SHIFT toggle
      _shift = !_shift;
    }
  }

  _compose_dirty = true;
  drawCompose();
}

static void handlePeersTouch(int16_t x, int16_t y) {
  (void)x; (void)y;
}

// =============================================================================
//  Public API
// =============================================================================
namespace UI {

  void begin(TFT_eSPI& tft_ref) {
    _tft = &tft_ref;
    _tft->setTextFont(1);
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  }

  void showScreen(ScreenID id) {
    _current_screen = id;
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BG);
    drawStatusBar();
    drawTabBar();
    if (id == SCREEN_INBOX)        drawInbox();
    else if (id == SCREEN_COMPOSE) { _compose_dirty = true; drawCompose(); }
    else                            drawPeers();
  }

  void handleTouch(int16_t x, int16_t y, bool pressed) {
    if (!pressed) { _was_pressed = false; return; }
    if (_was_pressed) return;
    uint32_t now = millis();
    if (now - _last_touch_ms < TOUCH_DEBOUNCE_MS) return;
    _last_touch_ms = now;
    _was_pressed = true;

    if (handleTabTouch(x, y)) return;

    switch (_current_screen) {
      case SCREEN_INBOX:   handleInboxTouch(x, y);   break;
      case SCREEN_COMPOSE: handleComposeTouch(x, y); break;
      case SCREEN_PEERS:   handlePeersTouch(x, y);   break;
    }
  }

  void update() {
    static uint32_t last_status_ms = 0;
    uint32_t now = millis();
    if (now - last_status_ms > 2000) {
      last_status_ms = now;
      drawStatusBar();
      // Refresh inbox timestamps every 2s
      if (_current_screen == SCREEN_INBOX) drawInbox();
    }
    if (_notify_visible) drawNotification();
  }

  void notifyNewMessage(const PagerMessage& msg) {
    if (msg.is_mine) {
      if (_current_screen == SCREEN_INBOX) drawInbox();
      return;
    }
    strncpy(_notify_sender, msg.sender_name, DEVICE_NAME_LEN - 1);
    strncpy(_notify_preview, msg.body, 47);
    _notify_visible   = true;
    _notify_start_ms  = millis();
    drawNotification();
    if (_current_screen == SCREEN_INBOX) drawInbox();
  }
}

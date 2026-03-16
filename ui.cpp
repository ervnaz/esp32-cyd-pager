// =============================================================================
//  ui.cpp  —  All screen drawing and touch handling
//  Screens:  INBOX | COMPOSE | PEERS
// =============================================================================
#include "ui.h"
#include "config.h"
#include "message_store.h"
#include "espnow_manager.h"
#include <TFT_eSPI.h>
#include <Arduino.h>

// ── Module-level state ────────────────────────────────────────────────────────
static TFT_eSPI* _tft = nullptr;
static ScreenID  _current_screen = SCREEN_INBOX;

// Touch debounce
static bool     _was_pressed     = false;
static uint32_t _last_touch_ms   = 0;

// Notification overlay
static bool     _notify_visible  = false;
static uint32_t _notify_start_ms = 0;
static char     _notify_sender[DEVICE_NAME_LEN] = "";
static char     _notify_preview[32] = "";

// Compose screen state
static char     _compose_buf[MAX_MSG_LEN] = "";
static int      _compose_len             = 0;
static bool     _shift                   = false;
static bool     _compose_dirty           = true;

// Inbox scroll
static int      _inbox_scroll = 0;   // message index of top visible

// ── Keyboard layout ──────────────────────────────────────────────────────────
// 4 rows, up to 10 keys each  (space = '_', backspace = '<', send = '!')
// Array width = KB_COLS+2 to hold 10 chars + null terminator safely
static const char KB_LOWER[KB_ROWS][KB_COLS+2] = {
  "qwertyuiop",
  "asdfghjkl.",
  "zxcvbnm,!<",  // ! = SEND,  < = BACKSPACE
  " _?123:;-' "  // space mapped to first char
};
static const char KB_UPPER[KB_ROWS][KB_COLS+2] = {
  "QWERTYUIOP",
  "ASDFGHJKL.",
  "ZXCVBNM,!<",
  " _?123:;-' "
};
static const char KB_NUM[KB_ROWS][KB_COLS+2] = {
  "1234567890",
  "!@#$%^&*()",
  "-_=+[]{}|<",
  " abc:;.,?! "
};
static bool _num_mode = false;

// ── Helpers ──────────────────────────────────────────────────────────────────
static void drawRect(int x, int y, int w, int h, uint16_t col) {
  _tft->drawRect(x, y, w, h, col);
}
static void fillRect(int x, int y, int w, int h, uint16_t col) {
  _tft->fillRect(x, y, w, h, col);
}
static void drawText(int x, int y, const char* txt, uint16_t col, uint8_t size=1) {
  _tft->setTextColor(col, 0x0000);
  _tft->setTextSize(size);
  _tft->setCursor(x, y);
  _tft->print(txt);
}

// ── Status bar (always visible) ──────────────────────────────────────────────
static void drawStatusBar() {
  fillRect(0, 0, SCREEN_W, STATUS_BAR_H, COL_PANEL);
  // Device name left
  _tft->setTextColor(COL_ACCENT, COL_PANEL);
  _tft->setTextSize(1);
  _tft->setCursor(4, 6);
  _tft->print(DEVICE_NAME);

  // Peer count centre
  char buf[20];
  int active = 0;
  for (int i = 0; i < ESPNowManager::peerCount(); i++)
    if (ESPNowManager::getPeer(i)->active) active++;
  snprintf(buf, sizeof(buf), "%d peer%s", active, active==1?"":"s");
  _tft->setTextColor(COL_TEXT_DIM, COL_PANEL);
  int tw = strlen(buf)*6;
  _tft->setCursor((SCREEN_W - tw)/2, 6);
  _tft->print(buf);

  // Unread badge right
  int unread = MessageStore::unread();
  if (unread > 0) {
    snprintf(buf, sizeof(buf), "[%d]", unread);
    _tft->setTextColor(COL_NOTIFY, COL_PANEL);
    _tft->setCursor(SCREEN_W - strlen(buf)*6 - 4, 6);
    _tft->print(buf);
  }
}

// ── Tab bar ──────────────────────────────────────────────────────────────────
static void drawTabBar() {
  int y = STATUS_BAR_H;
  fillRect(0, y, SCREEN_W, TAB_BAR_H, COL_PANEL);
  const char* labels[3] = {"INBOX", "COMPOSE", "PEERS"};
  int tw = SCREEN_W / 3;
  for (int i = 0; i < 3; i++) {
    bool active = (i == (int)_current_screen);
    fillRect(i*tw, y, tw, TAB_BAR_H, active ? COL_ACCENT : COL_PANEL);
    _tft->setTextColor(active ? COL_BG : COL_TEXT_DIM, active ? COL_ACCENT : COL_PANEL);
    _tft->setTextSize(1);
    int lx = i*tw + (tw - strlen(labels[i])*6)/2;
    _tft->setCursor(lx, y + 11);
    _tft->print(labels[i]);
    if (i < 2) drawRect(i*tw+tw-1, y, 1, TAB_BAR_H, COL_BG);
  }
}

// ── INBOX screen ─────────────────────────────────────────────────────────────
static void drawInbox() {
  fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  int count = MessageStore::count();

  if (count == 0) {
    _tft->setTextColor(COL_TEXT_DIM, COL_BG);
    _tft->setTextSize(1);
    _tft->setCursor(90, CONTENT_Y + CONTENT_H/2 - 4);
    _tft->print("No messages yet");
    return;
  }

  // Draw up to 4 message bubbles
  int bubble_h = CONTENT_H / 4;
  int y = CONTENT_Y + 2;

  for (int i = _inbox_scroll; i < count && i < _inbox_scroll + 4; i++) {
    const PagerMessage* m = MessageStore::get(i);
    if (!m) continue;

    bool mine = m->is_mine;
    uint16_t bub_col = mine ? COL_SENT : COL_RECV;
    int bx = mine ? 60 : 2;
    int bw = SCREEN_W - 64;

    fillRect(bx, y+1, bw, bubble_h-4, bub_col);

    // Sender + time row
    char hdr[32];
    uint32_t t = m->timestamp_s;
    snprintf(hdr, sizeof(hdr), "%s  +%us", mine ? "Me" : m->sender_name, t);
    _tft->setTextColor(mine ? COL_ACCENT : COL_ACCENT2, bub_col);
    _tft->setTextSize(1);
    _tft->setCursor(bx+3, y+3);
    _tft->print(hdr);

    // Message body (truncated to fit)
    _tft->setTextColor(COL_TEXT, bub_col);
    _tft->setCursor(bx+3, y+14);
    // Fit ~(bw/6) chars per line, 2 lines
    int maxChars = (bw / 6) * 2 - 2;
    char preview[120] = "";
    strncpy(preview, m->body, maxChars);
    if ((int)strlen(m->body) > maxChars) {
      preview[maxChars-2] = '.'; preview[maxChars-1] = '.'; preview[maxChars] = 0;
    }
    _tft->print(preview);

    // ACK tick for sent
    if (mine && m->ack_received) {
      _tft->setTextColor(COL_STATUS_OK, bub_col);
      _tft->setCursor(bx + bw - 10, y+3);
      _tft->print("*");
    }

    y += bubble_h;
  }

  // Scroll hint
  if (count > 4) {
    _tft->setTextColor(COL_TEXT_DIM, COL_BG);
    _tft->setTextSize(1);
    _tft->setCursor(SCREEN_W - 18, CONTENT_Y + CONTENT_H - 10);
    char sc[8]; snprintf(sc, sizeof(sc), "%d/%d", _inbox_scroll+1, count);
    _tft->print(sc);
  }
}

// ── COMPOSE screen ────────────────────────────────────────────────────────────
static const int KB_START_Y = CONTENT_Y + 42;   // below the text box
static const int KB_START_X = 0;

static void drawCompose() {
  if (!_compose_dirty) return;
  _compose_dirty = false;

  fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);

  // ── Text compose area ─────────────────────────────────────────────────────
  int box_h = 38;
  fillRect(2, CONTENT_Y+2, SCREEN_W-4, box_h, COL_PANEL);
  drawRect(2, CONTENT_Y+2, SCREEN_W-4, box_h, COL_ACCENT);

  // Prompt / char count
  char label[24];
  snprintf(label, sizeof(label), "MSG  %d/%d", _compose_len, MAX_MSG_LEN);
  _tft->setTextColor(COL_TEXT_DIM, COL_PANEL);
  _tft->setTextSize(1);
  _tft->setCursor(SCREEN_W-strlen(label)*6-4, CONTENT_Y+4);
  _tft->print(label);

  // Text with blinking cursor sim (just always show '|')
  _tft->setTextColor(COL_TEXT, COL_PANEL);
  _tft->setCursor(5, CONTENT_Y+8);

  // Show last 2 rows of text that fit
  int chars_per_row = (SCREEN_W-10) / 6;
  int start = (_compose_len > chars_per_row*2) ? _compose_len - chars_per_row*2 : 0;
  char view[80] = "";
  strncpy(view, _compose_buf + start, chars_per_row*2);

  // Split into 2 lines
  if ((int)strlen(view) > chars_per_row) {
    char l1[50]={}, l2[50]={};
    strncpy(l1, view, chars_per_row);
    strncpy(l2, view+chars_per_row, chars_per_row);
    _tft->setCursor(5, CONTENT_Y+8);  _tft->print(l1);
    _tft->setCursor(5, CONTENT_Y+20); _tft->print(l2);
  } else {
    _tft->setCursor(5, CONTENT_Y+14); _tft->print(view);
  }
  // Cursor bar
  _tft->setTextColor(COL_ACCENT, COL_PANEL);
  _tft->setCursor(5+(_compose_len % chars_per_row)*6, CONTENT_Y+26);
  _tft->print("_");

  // ── Keyboard ──────────────────────────────────────────────────────────────
  const char (*kb)[KB_COLS+2] = _num_mode ? KB_NUM : (_shift ? KB_UPPER : KB_LOWER);
  int key_w = SCREEN_W / KB_COLS;   // 32 px each

  for (int row = 0; row < KB_ROWS; row++) {
    for (int col = 0; col < KB_COLS; col++) {
      char c = kb[row][col];
      int kx = col * key_w;
      int ky = KB_START_Y + row * KB_KEY_H;
      int kw = key_w - 1;
      int kh = KB_KEY_H - 2;

      // Special key colours
      uint16_t bg = COL_KEY_BG;
      uint16_t fg = COL_TEXT;

      if (c == '<') { bg = 0xA000; fg = COL_TEXT; }       // backspace red
      if (c == '!' && row == 2) { bg = COL_STATUS_OK; fg = COL_BG; } // send green
      if (row == 3 && col == 0) { bg = COL_PANEL; }        // space bar

      fillRect(kx, ky, kw, kh, bg);
      drawRect(kx, ky, kw, kh, COL_BG);

      // Label
      char lbl[4] = {c, 0, 0, 0};
      if (c == '<') { lbl[0]='<'; lbl[1]='<'; lbl[2]=0; }
      if (c == '!' && row==2) { lbl[0]='S'; lbl[1]='N'; lbl[2]='D'; lbl[3]=0; }
      if (row==3 && col==0)   { lbl[0]='S'; lbl[1]='P'; lbl[2]='C'; lbl[3]=0; }

      _tft->setTextColor(fg, bg);
      _tft->setTextSize(1);
      int lx = kx + (kw - strlen(lbl)*6) / 2;
      int ly = ky + (kh - 8) / 2;
      _tft->setCursor(lx, ly);
      _tft->print(lbl);
    }
  }

  // Shift / Num mode indicators
  _tft->setTextColor(_shift ? COL_ACCENT : COL_TEXT_DIM, COL_BG);
  _tft->setCursor(2, KB_START_Y + KB_ROWS*KB_KEY_H + 2);
  _tft->print(_shift ? "SFT" : "sft");

  _tft->setTextColor(_num_mode ? COL_ACCENT2 : COL_TEXT_DIM, COL_BG);
  _tft->setCursor(24, KB_START_Y + KB_ROWS*KB_KEY_H + 2);
  _tft->print(_num_mode ? "NUM" : "num");
}

// ── Shift/Num toggle buttons (below keyboard) ─────────────────────────────────
static void drawComposeControls() {
  // Already drawn in drawCompose, kept for explicit redraw calls
  (void)0;
}

// ── PEERS screen ──────────────────────────────────────────────────────────────
static void drawPeers() {
  fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  _tft->setTextColor(COL_TEXT_DIM, COL_BG);
  _tft->setTextSize(1);
  _tft->setCursor(4, CONTENT_Y+4);
  _tft->print("Known peers:");

  int cnt = ESPNowManager::peerCount();
  if (cnt == 0) {
    _tft->setTextColor(COL_TEXT_DIM, COL_BG);
    _tft->setCursor(80, CONTENT_Y + 40);
    _tft->print("No peers seen yet");
    return;
  }

  int row_h = 22;
  for (int i = 0; i < cnt && i < 8; i++) {
    PeerInfo* p = ESPNowManager::getPeer(i);
    if (!p) continue;
    int py = CONTENT_Y + 16 + i*row_h;
    uint16_t row_col = p->active ? COL_PANEL : COL_BG;
    fillRect(2, py, SCREEN_W-4, row_h-2, row_col);

    // Status dot
    uint16_t dot_col = p->active ? COL_STATUS_OK : COL_STATUS_ERR;
    fillRect(5, py+7, 8, 8, dot_col);

    // Name
    _tft->setTextColor(p->active ? COL_TEXT : COL_TEXT_DIM, row_col);
    _tft->setTextSize(1);
    _tft->setCursor(18, py+7);
    _tft->print(p->name);

    // MAC
    char mac[20];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X", p->mac[3], p->mac[4], p->mac[5]);
    _tft->setTextColor(COL_TEXT_DIM, row_col);
    _tft->setCursor(SCREEN_W - 80, py+7);
    _tft->print(mac);

    // Last seen
    uint32_t age = (millis() - p->last_seen_ms) / 1000;
    char age_s[12];
    if (age < 60) snprintf(age_s, sizeof(age_s), "%us", age);
    else          snprintf(age_s, sizeof(age_s), "%um", age/60);
    _tft->setTextColor(COL_TEXT_DIM, row_col);
    _tft->setCursor(80, py+7);
    _tft->print(age_s);
  }
}

// ── Notification overlay ─────────────────────────────────────────────────────
static void drawNotification() {
  if (!_notify_visible) return;
  if (millis() - _notify_start_ms > NOTIFY_DISPLAY_MS) {
    // Erase and hide
    _notify_visible = false;
    // Redraw whatever screen was under it
    drawStatusBar();
    drawTabBar();
    if (_current_screen == SCREEN_INBOX)        drawInbox();
    else if (_current_screen == SCREEN_COMPOSE) { _compose_dirty=true; drawCompose(); }
    else                                         drawPeers();
    return;
  }

  int nx = 4, ny = CONTENT_Y + 4;
  int nw = SCREEN_W - 8, nh = 36;
  fillRect(nx, ny, nw, nh, COL_NOTIFY);
  drawRect(nx, ny, nw, nh, COL_TEXT);

  _tft->setTextColor(COL_BG, COL_NOTIFY);
  _tft->setTextSize(1);
  char hdr[28];
  snprintf(hdr, sizeof(hdr), "MSG FROM: %s", _notify_sender);
  _tft->setCursor(nx+4, ny+4);  _tft->print(hdr);
  _tft->setCursor(nx+4, ny+18); _tft->print(_notify_preview);
}

// ── Touch routing per screen ──────────────────────────────────────────────────
static void handleInboxTouch(int16_t x, int16_t y) {
  // Scroll: tap upper/lower 20% of content area
  if (y < CONTENT_Y + CONTENT_H/4 && _inbox_scroll > 0) {
    _inbox_scroll--;
    drawInbox();
  } else if (y > CONTENT_Y + 3*CONTENT_H/4) {
    if (_inbox_scroll < MessageStore::count() - 1) {
      _inbox_scroll++;
      drawInbox();
    }
  }
  MessageStore::clearUnread();
  drawStatusBar();
}

static void handleComposeTouch(int16_t x, int16_t y) {
  // Shift row (below keyboard)
  int cy = KB_START_Y + KB_ROWS * KB_KEY_H + 1;
  if (y >= cy) {
    if (x < 22) { _shift = !_shift; _compose_dirty = true; drawCompose(); return; }
    if (x < 44) { _num_mode = !_num_mode; _compose_dirty = true; drawCompose(); return; }
  }

  // Keyboard hit?
  if (y < KB_START_Y || y > KB_START_Y + KB_ROWS*KB_KEY_H) return;

  int key_w = SCREEN_W / KB_COLS;
  int col = x / key_w;
  int row = (y - KB_START_Y) / KB_KEY_H;
  if (row < 0 || row >= KB_ROWS || col < 0 || col >= KB_COLS) return;

  const char (*kb)[KB_COLS+2] = _num_mode ? KB_NUM : (_shift ? KB_UPPER : KB_LOWER);
  char c = kb[row][col];

  if (c == '<') {
    // Backspace
    if (_compose_len > 0) { _compose_buf[--_compose_len] = 0; }
  } else if (c == '!' && row == 2) {
    // SEND
    if (_compose_len > 0) {
      bool ok = ESPNowManager::sendMessage(_compose_buf);
      Serial.printf("[UI] Send %s: %s\n", ok?"OK":"FAIL", _compose_buf);
      memset(_compose_buf, 0, sizeof(_compose_buf));
      _compose_len = 0;
      if (ok) UI::showScreen(SCREEN_INBOX);
    }
  } else if (row == 3 && col == 0) {
    // Space
    if (_compose_len < MAX_MSG_LEN-1) _compose_buf[_compose_len++] = ' ';
  } else {
    if (_compose_len < MAX_MSG_LEN-1) _compose_buf[_compose_len++] = c;
    if (_shift && !_num_mode) _shift = false; // auto-unshift after one char
  }

  _compose_dirty = true;
  drawCompose();
}

static void handlePeersTouch(int16_t x, int16_t y) {
  // Nothing interactive on peers screen for now
  (void)x; (void)y;
}

// ── Tab bar touch ──────────────────────────────────────────────────────────────
static bool handleTabTouch(int16_t x, int16_t y) {
  if (y < STATUS_BAR_H || y > STATUS_BAR_H + TAB_BAR_H) return false;
  int tab = x / (SCREEN_W / 3);
  tab = constrain(tab, 0, 2);
  UI::showScreen((ScreenID)tab);
  return true;
}

// =============================================================================
//  Public API
// =============================================================================
namespace UI {

  void begin(TFT_eSPI& tft_ref) {
    _tft = &tft_ref;
    _tft->setTextFont(1);   // built-in 6x8 font
    fillRect(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  }

  void showScreen(ScreenID id) {
    _current_screen = id;
    fillRect(0, 0, SCREEN_W, SCREEN_H, COL_BG);
    drawStatusBar();
    drawTabBar();
    if (id == SCREEN_INBOX)        drawInbox();
    else if (id == SCREEN_COMPOSE) { _compose_dirty = true; drawCompose(); }
    else                            drawPeers();
  }

  void handleTouch(int16_t x, int16_t y, bool pressed) {
    if (!pressed) { _was_pressed = false; return; }
    if (_was_pressed) return;  // only fire on leading edge
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
    // Refresh status bar every 2 s (peer count / unread badge can change)
    if (now - last_status_ms > 2000) {
      last_status_ms = now;
      drawStatusBar();
    }
    // Dismiss notification overlay when its timer expires
    if (_notify_visible) drawNotification();
  }

  void notifyNewMessage(const PagerMessage& msg) {
    if (msg.is_mine) {
      // If we're not on inbox, refresh it silently
      if (_current_screen == SCREEN_INBOX) drawInbox();
      return;
    }
    // Show overlay
    strncpy(_notify_sender, msg.sender_name, DEVICE_NAME_LEN-1);
    strncpy(_notify_preview, msg.body, 31);
    _notify_visible   = true;
    _notify_start_ms  = millis();
    drawNotification();

    // Refresh inbox if currently visible
    if (_current_screen == SCREEN_INBOX) drawInbox();
  }
}

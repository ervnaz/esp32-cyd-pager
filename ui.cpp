// =============================================================================
//  ui.cpp  —  Display + touch handling
//  Screens: THE MESH | COMPOSE | PEERS
//  PEERS screen has a "RENAME" button that opens the keyboard to rename device
// =============================================================================
#include "ui.h"
#include "config.h"
#include "message_store.h"
#include "espnow_manager.h"
#include <TFT_eSPI.h>
#include <Arduino.h>

// ── State ─────────────────────────────────────────────────────────────────────
static TFT_eSPI* _tft = nullptr;
static ScreenID  _screen = SCREEN_MESH;

// Touch debounce
static bool     _was_pressed  = false;
static uint32_t _last_touch   = 0;

// Notification overlay
static bool     _notify_vis   = false;
static uint32_t _notify_start = 0;
static char     _notify_from[DEVICE_NAME_LEN] = "";
static char     _notify_prev[48] = "";

// Compose / rename shared buffer
static char     _buf[MAX_MSG_LEN] = "";
static int      _buf_len = 0;
static bool     _shift   = false;
static bool     _renaming = false;  // true = keyboard is for rename, not message

// ── Colour helpers ────────────────────────────────────────────────────────────
static void fillR(int x,int y,int w,int h,uint16_t c){ _tft->fillRect(x,y,w,h,c); }
static void drawR(int x,int y,int w,int h,uint16_t c){ _tft->drawRect(x,y,w,h,c); }
static void hline(int x,int y,int w,uint16_t c){ _tft->drawFastHLine(x,y,w,c); }
static void txt(int x,int y,const char* s,uint16_t fg,uint16_t bg=COL_BG){
  _tft->setTextColor(fg,bg); _tft->setTextSize(1); _tft->setCursor(x,y); _tft->print(s);
}

// ── Status bar ────────────────────────────────────────────────────────────────
static void drawStatus() {
  fillR(0, 0, SCREEN_W, STATUS_H, COL_PANEL);
  // Device name (left)
  _tft->setTextColor(COL_YELLOW, COL_PANEL);
  _tft->setTextSize(1);
  _tft->setCursor(4, 4);
  _tft->print(ESPNowManager::myName());
  // Peer count (centre)
  char buf[20];
  int active = ESPNowManager::activePeerCount();
  snprintf(buf, sizeof(buf), "%d peer%s", active, active == 1 ? "" : "s");
  _tft->setTextColor(COL_LTGRAY, COL_PANEL);
  _tft->setCursor((SCREEN_W - (int)strlen(buf) * 6) / 2, 4);
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

// ── Tab bar ───────────────────────────────────────────────────────────────────
static void drawTabs() {
  const char* labels[3] = {"THE MESH", "COMPOSE", "PEERS"};
  int tw = SCREEN_W / 3;
  fillR(0, TAB_Y, SCREEN_W, TAB_H, COL_DKBLUE);
  for (int i = 0; i < 3; i++) {
    bool active = (i == (int)_screen);
    if (active) fillR(i * tw, TAB_Y, tw, TAB_H, COL_ACTIVE);
    _tft->setTextColor(active ? COL_BG : COL_WHITE, active ? COL_ACTIVE : COL_DKBLUE);
    _tft->setTextSize(1);
    int lx = i * tw + (tw - (int)strlen(labels[i]) * 6) / 2;
    _tft->setCursor(lx, TAB_Y + 10);
    _tft->print(labels[i]);
    if (i < 2) _tft->drawFastVLine(i * tw + tw, TAB_Y, TAB_H, COL_BG);
  }
  hline(0, TAB_Y, SCREEN_W, COL_ACTIVE);
}

// ── Keyboard (shared between COMPOSE and RENAME) ──────────────────────────────
static void drawKeyboard(const char* prompt) {
  fillR(0, CON_Y, SCREEN_W, CON_H, COL_BG);

  // Text box
  fillR(2, CON_Y + 2, SCREEN_W - 4, 28, 0x0003);
  drawR(2, CON_Y + 2, SCREEN_W - 4, 28, COL_ACTIVE);
  _tft->setTextColor(COL_YELLOW, 0x0003);
  _tft->setCursor(4, CON_Y + 5);
  _tft->print(prompt);
  int px = strlen(prompt) * 6 + 4;
  _tft->setTextColor(COL_WHITE, 0x0003);
  _tft->setCursor(px, CON_Y + 5);
  _tft->print(_buf);
  // Char count
  char cc[12];
  int maxlen = _renaming ? (DEVICE_NAME_LEN - 1) : 100;
  snprintf(cc, sizeof(cc), "%d/%d", _buf_len, maxlen);
  _tft->setTextColor(COL_LTGRAY, 0x0003);
  _tft->setCursor(SCREEN_W - (int)strlen(cc) * 6 - 4, CON_Y + 5);
  _tft->print(cc);
  // Cursor
  int cx = px + _buf_len * 6;
  if (cx < SCREEN_W - 52) {
    _tft->setTextColor(COL_ACTIVE, 0x0003);
    _tft->setCursor(cx, CON_Y + 16);
    _tft->print("_");
  }

  // Keys
  const char* r1 = _shift ? "QWERTYUIOP" : "qwertyuiop";
  const char* r2 = _shift ? "ASDFGHJKL." : "asdfghjkl.";
  const char* r3 = _shift ? "ZXCVBNM,"   : "zxcvbnm,";

  for (int i = 0; i < 10; i++) {
    int kx = KB_KX + i * (KB_KW + KB_KG);
    fillR(kx, KB_KY, KB_KW, KB_KH, COL_DKBLUE);
    drawR(kx, KB_KY, KB_KW, KB_KH, COL_ACTIVE);
    char l[2] = {r1[i], 0};
    _tft->setTextColor(COL_WHITE, COL_DKBLUE);
    _tft->setCursor(kx + 9, KB_KY + 10); _tft->print(l);

    int ky2 = KB_KY + KB_KH + KB_KG;
    fillR(kx, ky2, KB_KW, KB_KH, COL_DKBLUE);
    drawR(kx, ky2, KB_KW, KB_KH, COL_ACTIVE);
    char l2[2] = {r2[i], 0};
    _tft->setTextColor(COL_WHITE, COL_DKBLUE);
    _tft->setCursor(kx + 9, ky2 + 10); _tft->print(l2);
  }
  int ky3 = KB_KY + 2 * (KB_KH + KB_KG);
  for (int i = 0; i < 8; i++) {
    int kx = KB_KX + i * (KB_KW + KB_KG);
    fillR(kx, ky3, KB_KW, KB_KH, COL_DKBLUE);
    drawR(kx, ky3, KB_KW, KB_KH, COL_ACTIVE);
    char l3[2] = {r3[i], 0};
    _tft->setTextColor(COL_WHITE, COL_DKBLUE);
    _tft->setCursor(kx + 9, ky3 + 10); _tft->print(l3);
  }
  // DEL
  int del_x = KB_KX + 8 * (KB_KW + KB_KG);
  fillR(del_x, ky3, KB_KW * 2 + KB_KG, KB_KH, COL_DKRED);
  drawR(del_x, ky3, KB_KW * 2 + KB_KG, KB_KH, COL_RED);
  _tft->setTextColor(COL_WHITE, COL_DKRED);
  _tft->setCursor(del_x + 8, ky3 + 10); _tft->print("DEL");

  // Row 4: SHF | SPACE | OK/SEND
  int ky4 = KB_KY + 3 * (KB_KH + KB_KG);
  fillR(KB_KX, ky4, KB_KW * 2 + KB_KG, KB_KH, _shift ? COL_ACTIVE : COL_DKBLUE);
  drawR(KB_KX, ky4, KB_KW * 2 + KB_KG, KB_KH, _shift ? COL_YELLOW : COL_ACTIVE);
  _tft->setTextColor(_shift ? COL_BG : COL_WHITE, _shift ? COL_ACTIVE : COL_DKBLUE);
  _tft->setCursor(KB_KX + 8, ky4 + 10); _tft->print("SHF");

  int spc_x = KB_KX + KB_KW * 2 + KB_KG * 2;
  fillR(spc_x, ky4, KB_KW * 5 + KB_KG * 4, KB_KH, COL_DKBLUE);
  drawR(spc_x, ky4, KB_KW * 5 + KB_KG * 4, KB_KH, COL_ACTIVE);
  _tft->setTextColor(COL_WHITE, COL_DKBLUE);
  _tft->setCursor(spc_x + 50, ky4 + 10); _tft->print("SPC");

  int ok_x = KB_KX + KB_KW * 7 + KB_KG * 7;
  uint16_t ok_bg = _renaming ? 0x03E0 : 0x0320;
  fillR(ok_x, ky4, KB_KW * 3 + KB_KG * 2, KB_KH, ok_bg);
  drawR(ok_x, ky4, KB_KW * 3 + KB_KG * 2, KB_KH, COL_GREEN);
  _tft->setTextColor(COL_BG, ok_bg);
  _tft->setCursor(ok_x + 6, ky4 + 10);
  _tft->print(_renaming ? "SAVE" : "SEND");
}

// ── THE MESH screen ───────────────────────────────────────────────────────────
static void drawMesh() {
  fillR(0, CON_Y, SCREEN_W, CON_H, COL_BG);
  txt(6, CON_Y + 2, "THE MESH", COL_ACTIVE, COL_BG);

  const int ROW_H = 15;
  const int ROW_G = 1;
  const int LIST_Y = CON_Y + 16;
  int count = MessageStore::count();
  int shown = 0;

  for (int i = 0; i < count && shown < MSG_HISTORY; i++) {
    const PagerMessage* m = MessageStore::get(i);
    if (!m) continue;
    int ry = LIST_Y + shown * (ROW_H + ROW_G);
    if (ry + ROW_H > TAB_Y) break;

    bool mine = m->is_mine;
    uint16_t row_bg  = mine ? COL_TEAL   : COL_DKBLUE;
    uint16_t lbl_col = mine ? COL_TEAL_LBL
                             : (shown == 0 ? COL_YELLOW : COL_LTGRAY);
    fillR(2, ry, SCREEN_W - 4, ROW_H, row_bg);

    char lbl[12];
    if (mine) snprintf(lbl, sizeof(lbl), ">>");
    else      snprintf(lbl, sizeof(lbl), "%.6s:", m->sender_name);

    int lbl_w = strlen(lbl) * 6;
    _tft->setTextColor(lbl_col, row_bg);
    _tft->setCursor(4, ry + 3);
    _tft->print(lbl);

    // Truncate body to fit
    char preview[48] = "";
    int avail = (SCREEN_W - 10 - lbl_w) / 6;
    strncpy(preview, m->body, min(avail, 47));

    _tft->setTextColor(COL_WHITE, row_bg);
    _tft->setCursor(4 + lbl_w + 3, ry + 3);
    _tft->print(preview);
    shown++;
  }

  if (shown == 0) {
    txt(6, CON_Y + 50, "No messages yet", COL_LTGRAY);
    txt(6, CON_Y + 66, "Go to COMPOSE to send one", COL_LTGRAY);
  }
}

// ── PEERS screen ──────────────────────────────────────────────────────────────
static void drawPeers() {
  fillR(0, CON_Y, SCREEN_W, CON_H, COL_BG);
  txt(4, CON_Y + 4, "Peers:", COL_YELLOW);
  hline(0, CON_Y + 16, SCREEN_W, COL_DKBLUE);

  int cnt = ESPNowManager::peerCount();
  int row_h = 22;
  for (int i = 0; i < cnt && i < 6; i++) {
    PeerInfo* p = ESPNowManager::getPeer(i);
    if (!p) continue;
    int py = CON_Y + 20 + i * row_h;
    uint16_t rc = p->active ? COL_DKBLUE : COL_BG;
    fillR(2, py, SCREEN_W - 4, row_h - 2, rc);
    _tft->fillRect(5, py + 7, 8, 8, p->active ? COL_GREEN : COL_RED);
    _tft->setTextColor(p->active ? COL_WHITE : COL_LTGRAY, rc);
    _tft->setCursor(18, py + 7);
    _tft->print(p->name);
    // Partial MAC
    char mac[10];
    snprintf(mac, sizeof(mac), "%02X%02X%02X", p->mac[3], p->mac[4], p->mac[5]);
    _tft->setTextColor(COL_LTGRAY, rc);
    _tft->setCursor(SCREEN_W - 64, py + 7);
    _tft->print(mac);
    // Age
    uint32_t age = (millis() - p->last_seen_ms) / 1000;
    char age_s[10];
    if (age < 60) snprintf(age_s, sizeof(age_s), "%us", age);
    else          snprintf(age_s, sizeof(age_s), "%um", age/60);
    _tft->setCursor(80, py + 7);
    _tft->print(age_s);
  }
  if (cnt == 0) txt(6, CON_Y + 30, "No peers seen yet", COL_LTGRAY);

  // RENAME button at bottom of content
  int btn_y = TAB_Y - 30;
  fillR(4, btn_y, 140, 24, COL_DKBLUE);
  drawR(4, btn_y, 140, 24, COL_ACTIVE);
  txt(10, btn_y + 7, "RENAME THIS DEVICE", COL_ACTIVE, COL_DKBLUE);

  // Show current name
  char info[32];
  snprintf(info, sizeof(info), "This device: %s", ESPNowManager::myName());
  txt(4, btn_y - 14, info, COL_LTGRAY);
}

// ── Notification overlay ──────────────────────────────────────────────────────
static void drawNotification() {
  if (!_notify_vis) return;
  if (millis() - _notify_start > NOTIFY_DISPLAY_MS) {
    _notify_vis = false;
    UI::showScreen(_screen);
    return;
  }
  fillR(4, CON_Y + 2, SCREEN_W - 8, 40, COL_ORANGE);
  drawR(4, CON_Y + 2, SCREEN_W - 8, 40, COL_WHITE);
  char hdr[32];
  snprintf(hdr, sizeof(hdr), "MSG FROM: %s", _notify_from);
  _tft->setTextColor(COL_BG, COL_ORANGE);
  _tft->setCursor(8, CON_Y + 6);  _tft->print(hdr);
  _tft->setCursor(8, CON_Y + 20); _tft->print(_notify_prev);
}

// ── Touch: keyboard handler (shared for compose + rename) ─────────────────────
static void handleKeyboardTouch(int16_t x, int16_t y) {
  const int KX = KB_KX, KY = KB_KY, KW = KB_KW, KH = KB_KH, KG = KB_KG;
  if (y < KY || y > KY + 4 * (KH + KG)) return;

  int row = (y - KY) / (KH + KG);
  int col = (x - KX) / (KW + KG);
  if (col < 0 || col > 9) return;

  const char* up[3] = {"QWERTYUIOP", "ASDFGHJKL.", "ZXCVBNM,"};
  const char* lo[3] = {"qwertyuiop", "asdfghjkl.", "zxcvbnm,"};
  const char** rows = _shift ? up : lo;
  int maxlen = _renaming ? (DEVICE_NAME_LEN - 1) : 100;

  if (row == 0 && col < 10) {
    if (_buf_len < maxlen) { _buf[_buf_len++] = rows[0][col]; _buf[_buf_len] = 0; _shift = false; }
  } else if (row == 1 && col < 10) {
    if (_buf_len < maxlen) { _buf[_buf_len++] = rows[1][col]; _buf[_buf_len] = 0; _shift = false; }
  } else if (row == 2) {
    if (col < 8) {
      if (_buf_len < maxlen) { _buf[_buf_len++] = rows[2][col]; _buf[_buf_len] = 0; _shift = false; }
    } else {
      if (_buf_len > 0) _buf[--_buf_len] = 0;
    }
  } else if (row == 3) {
    int ok_x  = KX + KW * 7 + KG * 7;
    int spc_x = KX + KW * 2 + KG * 2;
    if (x >= ok_x) {
      // SEND or SAVE
      if (_buf_len > 0) {
        if (_renaming) {
          ESPNowManager::setName(_buf);
          _renaming = false;
          memset(_buf, 0, sizeof(_buf));
          _buf_len = 0;
          _shift = false;
          UI::showScreen(SCREEN_PEERS);
        } else {
          ESPNowManager::sendMessage(_buf);
          memset(_buf, 0, sizeof(_buf));
          _buf_len = 0;
          _shift = false;
          UI::showScreen(SCREEN_MESH);
        }
        return;
      }
    } else if (x >= spc_x) {
      if (_buf_len < maxlen) { _buf[_buf_len++] = ' '; _buf[_buf_len] = 0; }
    } else {
      _shift = !_shift;
    }
  }
  drawKeyboard(_renaming ? "NAME: " : "MSG: ");
}

// =============================================================================
//  Public API
// =============================================================================
namespace UI {

  void begin(TFT_eSPI& tft_ref) {
    _tft = &tft_ref;
    _tft->setTextFont(1);
    fillR(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  }

  void showScreen(ScreenID id) {
    _screen = id;
    fillR(0, 0, SCREEN_W, SCREEN_H, COL_BG);
    drawStatus();
    drawTabs();
    switch (id) {
      case SCREEN_MESH:
        MessageStore::clearUnread();
        drawMesh();
        break;
      case SCREEN_COMPOSE:
        _renaming = false;
        drawKeyboard("MSG: ");
        break;
      case SCREEN_PEERS:
        drawPeers();
        break;
    }
  }

  void handleTouch(int16_t x, int16_t y, bool pressed) {
    if (!pressed) { _was_pressed = false; return; }
    if (_was_pressed) return;
    uint32_t now = millis();
    if (now - _last_touch < TOUCH_DEBOUNCE_MS) return;
    _last_touch = now;
    _was_pressed = true;

    // Tab bar
    if (y >= TAB_Y - 4 && y <= SCREEN_H) {
      int tab = x / (SCREEN_W / 3);
      tab = constrain(tab, 0, 2);
      // If we were renaming, cancel it
      if (_renaming) {
        _renaming = false;
        memset(_buf, 0, sizeof(_buf));
        _buf_len = 0;
        _shift = false;
      }
      showScreen((ScreenID)tab);
      return;
    }

    // Screen-specific touch
    switch (_screen) {
      case SCREEN_MESH:
        MessageStore::clearUnread();
        drawStatus();
        break;
      case SCREEN_COMPOSE:
        handleKeyboardTouch(x, y);
        break;
      case SCREEN_PEERS: {
        // RENAME button zone
        int btn_y = TAB_Y - 30;
        if (y >= btn_y && y <= btn_y + 24 && x >= 4 && x <= 144) {
          // Enter rename mode using the compose keyboard
          _renaming = true;
          memset(_buf, 0, sizeof(_buf));
          _buf_len = 0;
          _shift = true;   // start with shift on for name
          _screen = SCREEN_COMPOSE;  // reuse compose tab visually
          fillR(0, 0, SCREEN_W, SCREEN_H, COL_BG);
          drawStatus();
          drawTabs();
          drawKeyboard("NAME: ");
        }
        break;
      }
    }
  }

  void update() {
    static uint32_t last_status = 0;
    uint32_t now = millis();
    if (now - last_status > 2000) {
      last_status = now;
      drawStatus();
    }
    if (_notify_vis) drawNotification();
  }

  void notifyNewMessage(const PagerMessage& msg) {
    if (msg.is_mine) {
      if (_screen == SCREEN_MESH) drawMesh();
      return;
    }
    strncpy(_notify_from, msg.sender_name, DEVICE_NAME_LEN - 1);
    strncpy(_notify_prev, msg.body, 47);
    _notify_vis   = true;
    _notify_start = millis();
    drawNotification();
    if (_screen == SCREEN_MESH) drawMesh();
  }
}

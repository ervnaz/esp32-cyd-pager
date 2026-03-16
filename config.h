// =============================================================================
//  config.h  —  All tunable constants for the CYD Pager
// =============================================================================
#pragma once
#include <stdint.h>
#include <Arduino.h>

// ── Device identity ──────────────────────────────────────────────────────────
// Set a unique 1-8 char name for each unit before flashing
#define DEVICE_NAME   "UNIT-1"

// ── Screen ───────────────────────────────────────────────────────────────────
#define SCREEN_W        320
#define SCREEN_H        240
#define SCREEN_ROTATION   1    // 0=portrait, 1=landscape, 2/3=flipped variants

// ── Backlight ────────────────────────────────────────────────────────────────
#define TFT_BL          21     // CYD backlight pin

// ── Touch SPI pins (XPT2046 on CYD) ─────────────────────────────────────────
#define XPT_CLK         25
#define XPT_MOSI        32
#define XPT_MISO        39
// TOUCH_CS and TOUCH_IRQ are defined in the main .ino

#define TOUCH_ROTATION   1

// XPT2046 raw ADC calibration  (run touch_calibrate sketch to get your values)
#define TOUCH_X_MIN    200
#define TOUCH_X_MAX   3800
#define TOUCH_Y_MIN    300
#define TOUCH_Y_MAX   3700
#define TOUCH_SWAP_XY  false   // set true if axes appear swapped

// ── ESP-NOW ──────────────────────────────────────────────────────────────────
#define ESPNOW_CHANNEL   1     // WiFi channel (all peers must match)
// Broadcast address – sends to ALL peers in range
static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// Max peers to remember (ESP-NOW hard limit is 20)
#define MAX_PEERS        10

// ── Message limits ───────────────────────────────────────────────────────────
#define MAX_MSG_LEN      200   // chars per message body
#define MAX_MESSAGES     30    // inbox capacity (oldest dropped when full)
#define DEVICE_NAME_LEN   9    // incl. null terminator

// ── UI layout ────────────────────────────────────────────────────────────────
#define STATUS_BAR_H     20
#define TAB_BAR_H        30
#define CONTENT_Y        (STATUS_BAR_H + TAB_BAR_H)
#define CONTENT_H        (SCREEN_H - CONTENT_Y)

// Keyboard
#define KB_KEY_W         26
#define KB_KEY_H         28
#define KB_ROWS           4
#define KB_COLS          10

// ── Colours (RGB565) ─────────────────────────────────────────────────────────
#define COL_BG          0x0841   // very dark blue-grey
#define COL_PANEL       0x1082   // slightly lighter panel
#define COL_ACCENT      0x07FF   // cyan
#define COL_ACCENT2     0xF81F   // magenta
#define COL_TEXT        0xFFFF   // white
#define COL_TEXT_DIM    0x8410   // mid-grey
#define COL_SENT        0x0411   // dark teal bubble
#define COL_RECV        0x2104   // dark olive bubble
#define COL_KEY_BG      0x2945   // keyboard key background
#define COL_KEY_PRESS   0x07FF   // key highlight
#define COL_STATUS_OK   0x07E0   // green
#define COL_STATUS_ERR  0xF800   // red
#define COL_NOTIFY      0xFD20   // orange notification badge

// ── Timing ───────────────────────────────────────────────────────────────────
#define TOUCH_DEBOUNCE_MS    80
#define NOTIFY_DISPLAY_MS  3000
#define PEER_TIMEOUT_MS   30000  // remove peer from list after 30 s silence
#define HEARTBEAT_MS       5000  // broadcast heartbeat interval

// ── Helper: map raw touch ADC → screen pixels ────────────────────────────────
struct TouchPoint { int16_t x; int16_t y; };

inline TouchPoint mapTouch(int16_t rx, int16_t ry) {
  TouchPoint tp;
#if TOUCH_SWAP_XY
  int16_t tmp = rx; rx = ry; ry = tmp;
#endif
  tp.x = map(rx, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1);
  tp.y = map(ry, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1);
  tp.x = constrain(tp.x, 0, SCREEN_W - 1);
  tp.y = constrain(tp.y, 0, SCREEN_H - 1);
  return tp;
}

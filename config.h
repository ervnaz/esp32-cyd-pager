// =============================================================================
//  config.h  —  All tunable constants for the CYD Pager
//  Edit DEVICE_NAME before flashing each unit
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
#define SCREEN_ROTATION   1    // 1 = landscape

// ── Backlight ────────────────────────────────────────────────────────────────
#define TFT_BL          21

// ── TFT SPI pins ─────────────────────────────────────────────────────────────
// Variant A (most common CYD):  MOSI=23, MISO=19, CLK=18
// Variant B (some units):       MOSI=13, MISO=12, CLK=14
// Uncomment the variant that matches your board:
#define TFT_MOSI  23
#define TFT_MISO  19
#define TFT_SCLK  18
// #define TFT_MOSI  13
// #define TFT_MISO  12
// #define TFT_SCLK  14

// ── Touch SPI pins (XPT2046) — same on all CYD variants ─────────────────────
#define XPT_CLK         25
#define XPT_MOSI        32
#define XPT_MISO        39
#define TOUCH_CS        33
#define TOUCH_IRQ       36
#define TOUCH_ROTATION   1

// XPT2046 raw ADC calibration  (run TFT_eSPI Touch_calibrate to get yours)
#define TOUCH_X_MIN    200
#define TOUCH_X_MAX   3800
#define TOUCH_Y_MIN    300
#define TOUCH_Y_MAX   3700
#define TOUCH_SWAP_XY  true    // swap axes for landscape

// ── ESP-NOW ──────────────────────────────────────────────────────────────────
#define ESPNOW_CHANNEL   1
static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
#define MAX_PEERS        10

// ── Message limits ───────────────────────────────────────────────────────────
#define MAX_MSG_LEN      200
#define MAX_MESSAGES     30    // ring-buffer capacity
#define MSG_HISTORY        5   // messages shown in INBOX
#define DEVICE_NAME_LEN    9   // incl. null terminator

// ── UI layout (landscape 320x240) ────────────────────────────────────────────
#define STATUS_H    18         // status bar height
#define TAB_H       28         // tab bar height
#define TAB_Y       (SCREEN_H - TAB_H)   // tab bar at bottom y=212
#define CON_Y       STATUS_H   // content area top y=18
#define CON_H       (TAB_Y - CON_Y)      // content area height

// Keyboard geometry
#define KB_KW       31         // key width
#define KB_KH       30         // key height
#define KB_KG        1         // key gap
#define KB_KX        2         // keyboard left margin
#define KB_KY       (CON_Y + 34)  // keyboard top (below text box)

// ── Colours (RGB565) ─────────────────────────────────────────────────────────
#define COL_BG       0x0000   // black
#define COL_PANEL    0x0018   // dark blue panel
#define COL_ACTIVE   0x07FF   // cyan — active tab / accent
#define COL_WHITE    0xFFFF   // primary text
#define COL_YELLOW   0xFFE0   // labels / headings
#define COL_ORANGE   0xFD00   // unread badge / notification
#define COL_GREEN    0x07E0   // online indicator
#define COL_RED      0xF800   // DEL key
#define COL_LTGRAY   0xC618   // secondary text
#define COL_DKBLUE   0x000F   // bubble backgrounds
#define COL_DKRED    0x8000   // DEL key background

// ── Timing ───────────────────────────────────────────────────────────────────
#define TOUCH_DEBOUNCE_MS    80
#define NOTIFY_DISPLAY_MS  3000
#define PEER_TIMEOUT_MS   30000
#define HEARTBEAT_MS       5000

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

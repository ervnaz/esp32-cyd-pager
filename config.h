// =============================================================================
//  config.h  —  CYD Pager — standalone mesh, no WiFi / OTA / HA
//
//  Device name is set automatically from MAC address on first boot.
//  You can rename the device from the PEERS screen on-device.
//  Edit MESH_CHANNEL if you need to avoid interference (1, 6, or 11).
// =============================================================================
#pragma once
#include <stdint.h>
#include <Arduino.h>

// ── Mesh channel — ALL units must use the same value ─────────────────────────
#define MESH_CHANNEL  1           // 1, 6, or 11 recommended

// ── Encryption keys — ALL units must match ───────────────────────────────────
// Generate your own:
//   python3 -c "import os;k=os.urandom(16).hex();print(','.join(f'0x{k[i:i+2]}' for i in range(0,32,2)))"
#define PAGER_PMK_BYTES \
  0xA3,0x5C,0x82,0xF1,0x47,0xD9,0x0E,0x6B, \
  0x23,0x91,0xC4,0x58,0x7A,0x1F,0xE6,0x3D
#define PAGER_LMK_BYTES \
  0x71,0xB8,0x2A,0xC0,0x95,0x4E,0x3F,0xD7, \
  0x08,0xEC,0x62,0xA5,0x1D,0x83,0xF0,0x49

// ── Screen ───────────────────────────────────────────────────────────────────
#define SCREEN_W        320
#define SCREEN_H        240
#define SCREEN_ROTATION   1    // landscape

// ── Backlight ────────────────────────────────────────────────────────────────
#define TFT_BL          21

// ── TFT SPI pins ─────────────────────────────────────────────────────────────
// Variant A (most common CYD): MOSI=23, MISO=19, CLK=18
// Variant B (some units):      MOSI=13, MISO=12, CLK=14
// Uncomment the block matching your board:
#define TFT_MOSI  23
#define TFT_MISO  19
#define TFT_SCLK  18
// #define TFT_MOSI  13
// #define TFT_MISO  12
// #define TFT_SCLK  14

#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1

// ── Touch SPI pins (same on all CYD variants) ────────────────────────────────
#define XPT_CLK         25
#define XPT_MOSI        32
#define XPT_MISO        39
#define TOUCH_CS        33
#define TOUCH_IRQ       36
#define TOUCH_ROTATION   1

// XPT2046 calibration (run TFT_eSPI Touch_calibrate sketch to get yours)
#define TOUCH_X_MIN    200
#define TOUCH_X_MAX   3800
#define TOUCH_Y_MIN    300
#define TOUCH_Y_MAX   3700
#define TOUCH_SWAP_XY  true

// ── Message limits ───────────────────────────────────────────────────────────
#define MAX_MSG_LEN      200
#define MAX_MESSAGES      30
#define MSG_HISTORY       10
#define DEVICE_NAME_LEN    9   // 8 chars + null

// ── UI layout (landscape 320x240) ────────────────────────────────────────────
#define STATUS_H    18
#define TAB_H       28
#define TAB_Y       (SCREEN_H - TAB_H)   // tab bar at bottom  y=212
#define CON_Y       STATUS_H              // content area top   y=18
#define CON_H       (TAB_Y - CON_Y)      // content height

// Keyboard geometry
#define KB_KW       31
#define KB_KH       30
#define KB_KG        1
#define KB_KX        2
#define KB_KY       (CON_Y + 34)

// ── Colours (RGB565) ─────────────────────────────────────────────────────────
#define COL_BG       0x0000
#define COL_PANEL    0x000F   // dark blue
#define COL_ACTIVE   0x07FF   // cyan
#define COL_WHITE    0xFFFF
#define COL_YELLOW   0xFFE0
#define COL_ORANGE   0xFD00
#define COL_GREEN    0x07E0
#define COL_RED      0xF800
#define COL_LTGRAY   0xC618
#define COL_DKBLUE   0x000F
#define COL_DKRED    0x8000
#define COL_TEAL     0x0410   // sent message background
#define COL_TEAL_LBL 0x07BA   // sent message label

// ── Timing ───────────────────────────────────────────────────────────────────
#define TOUCH_DEBOUNCE_MS    80
#define NOTIFY_DISPLAY_MS  3000
#define PEER_TIMEOUT_MS   30000
#define HEARTBEAT_MS       5000

// ── Touch coordinate mapping ─────────────────────────────────────────────────
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

// =============================================================================
//  esp32_pager.ino  —  ESP-NOW mesh pager for ESP32-2432S028R (CYD)
//
//  Features:
//    - Message history (last 5 received, with relative timestamps)
//    - On-screen keyboard with SHIFT toggle (upper/lower case)
//    - Unread badge with auto-clear on INBOX open
//    - Auto peer discovery via heartbeat beacons
//    - Dynamic WiFi channel matching (compatible with ESPHome units)
//    - Notification banner on incoming messages
//    - Three screens: INBOX | COMPOSE | PEERS
//
//  Libraries required:
//    - TFT_eSPI by Bodmer          (copy User_Setup.h to library folder)
//    - XPT2046_Touchscreen by Paul Stoffregen
//
//  Per-unit config: edit DEVICE_NAME in config.h before flashing
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "config.h"
#include "ui.h"
#include "espnow_manager.h"
#include "message_store.h"

TFT_eSPI             tft = TFT_eSPI();
XPT2046_Touchscreen  ts(TOUCH_CS, TOUCH_IRQ);

// Forward declaration
void onMessageReceived(const PagerMessage& msg);

// =============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[PAGER] Booting...");

  // Display
  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(COL_BG);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Touch (separate HSPI bus)
  SPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(TOUCH_ROTATION);

  // WiFi in STA mode (needed for ESP-NOW, no router required)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("[PAGER] MAC: ");
  Serial.println(WiFi.macAddress());

  // ESP-NOW
  ESPNowManager::begin(onMessageReceived);

  // UI
  UI::begin(tft);
  UI::showScreen(SCREEN_INBOX);

  Serial.println("[PAGER] Ready.");
}

// =============================================================================
void loop() {
  // Touch handling
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point raw = ts.getPoint();
    TouchPoint tp = mapTouch(raw.x, raw.y);
    UI::handleTouch(tp.x, tp.y, true);
  } else {
    UI::handleTouch(0, 0, false);
  }

  // Periodic tasks (status bar refresh, notification dismiss, heartbeat)
  UI::update();
  ESPNowManager::update();
}

// =============================================================================
//  ESP-NOW receive callback — called from WiFi task, posts to MessageStore
// =============================================================================
void onMessageReceived(const PagerMessage& msg) {
  MessageStore::add(msg);
  UI::notifyNewMessage(msg);
}

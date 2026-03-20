// =============================================================================
//  esp32_pager.ino  —  Standalone ESP-NOW mesh pager
//  Board: ESP32-2432S028R ("Cheap Yellow Display")
//
//  NO WiFi router required. NO internet. NO OTA. NO Home Assistant.
//  Devices find each other automatically and communicate directly.
//
//  Device name is auto-generated from MAC address on first boot (e.g. ND-A3F2C1)
//  and can be changed from the PEERS screen on-device.
//
//  Libraries required:
//    - TFT_eSPI by Bodmer         (copy User_Setup.h to library folder)
//    - XPT2046_Touchscreen by Paul Stoffregen
// =============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "config.h"
#include "ui.h"
#include "espnow_manager.h"
#include "message_store.h"

TFT_eSPI            tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

void onMessage(const PagerMessage& msg);

// =============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[PAGER] Booting — standalone mesh mode");

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

  // Load persisted messages before UI starts
  MessageStore::load();

  // ESP-NOW (WiFi radio only — no router, no connect)
  ESPNowManager::begin(onMessage);

  // UI
  UI::begin(tft);
  UI::showScreen(SCREEN_MESH);

  Serial.println("[PAGER] Ready.");
}

// =============================================================================
void loop() {
  // Touch
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point raw = ts.getPoint();
    TouchPoint tp = mapTouch(raw.x, raw.y);
    UI::handleTouch(tp.x, tp.y, true);
  } else {
    UI::handleTouch(0, 0, false);
  }

  // Heartbeat + peer expiry
  ESPNowManager::update();

  // Status bar refresh + notification dismiss
  UI::update();
}

// =============================================================================
void onMessage(const PagerMessage& msg) {
  MessageStore::add(msg);
  UI::notifyNewMessage(msg);
}

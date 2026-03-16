// =============================================================================
//  ESP32 CYD Pager  —  ESP-NOW mesh messaging
//  Board: ESP32-2432S028R ("Cheap Yellow Display")
//  Libs : TFT_eSPI, XPT2046_Touchscreen, esp_now (built-in)
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// ---------- Touch: pick ONE block and comment the other ----------
// Option A – XPT2046 resistive (most CYD boards)
#include <XPT2046_Touchscreen.h>
#define TOUCH_CS  33
#define TOUCH_IRQ 36
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
// ----------------------------------------------------------------
// Option B – CST816S capacitive (some newer CYD variants)
// #include <CST816S.h>
// CST816S ts(21, 22, 4, 5);   // SDA, SCL, RST, IRQ  — adjust pins
// ----------------------------------------------------------------

#include "config.h"
#include "ui.h"
#include "espnow_manager.h"
#include "message_store.h"

TFT_eSPI tft = TFT_eSPI();

// ── forward declarations ──────────────────────────────────────────────────────
void onMessageReceived(const PagerMessage& msg);

// =============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[PAGER] Booting...");

  // ── Display init ────────────────────────────────────────────────────────────
  tft.init();
  tft.setRotation(SCREEN_ROTATION);   // landscape
  tft.fillScreen(TFT_BLACK);

  // Backlight (GPIO 21 on CYD)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // ── Touch init ──────────────────────────────────────────────────────────────
  // Option A – XPT2046
  SPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(TOUCH_ROTATION);
  // Option B – CST816S  →  ts.begin();

  // ── WiFi / ESP-NOW init ─────────────────────────────────────────────────────
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Print this device's MAC so you can add it as a peer on other units
  Serial.print("[PAGER] MAC: ");
  Serial.println(WiFi.macAddress());

  ESPNowManager::begin(onMessageReceived);

  // ── UI init ─────────────────────────────────────────────────────────────────
  UI::begin(tft);
  UI::showScreen(SCREEN_INBOX);

  Serial.println("[PAGER] Ready.");
}

// =============================================================================
void loop() {
  // ── Touch handling ──────────────────────────────────────────────────────────
  // Option A – XPT2046
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point raw = ts.getPoint();
    TouchPoint tp = mapTouch(raw.x, raw.y);
    UI::handleTouch(tp.x, tp.y, true);
  } else {
    UI::handleTouch(0, 0, false);
  }

  // Option B – CST816S
  // if (ts.available()) {
  //   UI::handleTouch(ts.data.x, ts.data.y, true);
  // } else {
  //   UI::handleTouch(0, 0, false);
  // }

  // ── Periodic tasks ──────────────────────────────────────────────────────────
  UI::update();
  ESPNowManager::update();
}

// =============================================================================
//  ESP-NOW receive callback  (called from ISR context → post to queue)
// =============================================================================
void onMessageReceived(const PagerMessage& msg) {
  MessageStore::add(msg);
  UI::notifyNewMessage(msg);
}

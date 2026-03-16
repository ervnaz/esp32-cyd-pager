// =============================================================================
//  ui.h  —  UI public API
// =============================================================================
#pragma once
#include <TFT_eSPI.h>
#include "message_types.h"

enum ScreenID {
  SCREEN_INBOX   = 0,
  SCREEN_COMPOSE = 1,
  SCREEN_PEERS   = 2,
};

namespace UI {
  void begin(TFT_eSPI& tft_ref);
  void showScreen(ScreenID id);
  void handleTouch(int16_t x, int16_t y, bool pressed);
  void update();
  void notifyNewMessage(const PagerMessage& msg);
}

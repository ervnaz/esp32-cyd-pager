// =============================================================================
//  espnow_manager.h  —  Public API only (implementation in espnow_manager.cpp)
// =============================================================================
#pragma once
#include "message_types.h"
#include "config.h"

namespace ESPNowManager {
  void      begin(void (*rx_cb)(const PagerMessage&));
  bool      sendMessage(const char* text);
  void      setName(const char* name);
  void      update();
  const char* myName();
  int       peerCount();
  int       activePeerCount();
  PeerInfo* getPeer(int i);
}

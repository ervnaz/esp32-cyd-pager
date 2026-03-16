# ESP32 CYD Pager

Standalone mesh pager using **ESP-NOW** on the **ESP32-2432S028R** ("Cheap Yellow Display").  
No WiFi router, no internet — devices talk directly to each other.

---

## Hardware

| Board | ESP32-2432S028R (CYD) |
|---|---|
| Display | 2.8" ILI9341 TFT 320×240 |
| Touch | XPT2046 resistive (default) |
| Radio | ESP-NOW over 2.4 GHz WiFi radio |
| Range | ~100 m open air (ESP-NOW typical) |

---

## Required Libraries (Arduino IDE / PlatformIO)

| Library | Install via |
|---|---|
| **TFT_eSPI** by Bodmer | Library Manager |
| **XPT2046_Touchscreen** by Paul Stoffregen | Library Manager |
| **esp_now** | Built into ESP32 Arduino core |
| **WiFi** | Built into ESP32 Arduino core |

> **For CST816S capacitive touch** (alternative boards):  
> Install **CST816S** by koendv — uncomment Option B lines in `.ino` and comment Option A.

---

## TFT_eSPI Setup (IMPORTANT)

Copy `User_Setup.h` from this project into your TFT_eSPI library folder,
replacing the existing one:

```
Arduino/libraries/TFT_eSPI/User_Setup.h
```

Or use `User_Setup_Select.h` to point to a custom path.

---

## Wiring Reference (CYD built-in — no external wiring needed)

The CYD has everything soldered on-board:

| Function | GPIO |
|---|---|
| TFT MOSI | 23 |
| TFT MISO | 19 |
| TFT CLK  | 18 |
| TFT CS   | 15 |
| TFT DC   | 2  |
| TFT BL   | 21 |
| Touch CLK| 25 |
| Touch MOSI| 32 |
| Touch MISO| 39 |
| Touch CS | 33 |
| Touch IRQ| 36 |

---

## Per-Device Configuration

Before flashing each unit, edit **`config.h`**:

```cpp
#define DEVICE_NAME   "UNIT-1"   // unique name, max 8 chars
```

Each device auto-discovers peers when they come online — no manual MAC entry needed.

---

## Touch Calibration

The constants in `config.h` are approximate:

```cpp
#define TOUCH_X_MIN    200
#define TOUCH_X_MAX   3800
#define TOUCH_Y_MIN    300
#define TOUCH_Y_MAX   3700
```

To calibrate for your specific board, run the **TFT_eSPI Touch_calibrate** example sketch
and paste the output values here.  If the axes are swapped set `TOUCH_SWAP_XY true`.

---

## ESP-NOW Channel

All devices **must use the same WiFi channel**.  Default is channel **1** (`ESPNOW_CHANNEL` in config.h).  
If you run into interference, change all units to channel 6 or 11.

---

## Project File Structure

```
esp32_pager/
├── esp32_pager.ino      ← Main sketch (open this in Arduino IDE)
├── config.h             ← All pins, timing, colours — edit per unit
├── message_types.h      ← Shared data structures (wire format)
├── message_store.h      ← Inbox ring-buffer
├── espnow_manager.h     ← ESP-NOW send/receive/peer management
├── ui.h                 ← UI public API
├── ui.cpp               ← All screen drawing & touch handling
└── User_Setup.h         ← TFT_eSPI config → copy to library folder
```

---

## UI Overview

### Screens (tap the tab bar to switch)

| Tab | Description |
|---|---|
| **INBOX** | Message bubbles, newest first. Tap top/bottom 25% to scroll. |
| **COMPOSE** | On-screen keyboard. Tap **SND** key (green) to broadcast. |
| **PEERS** | Live peer list with last-seen time and online status dot. |

### Status Bar (always visible)
- **Left**: This device's name
- **Centre**: Active peer count
- **Right**: Unread message count (orange `[N]`)

### Notification Banner
Incoming messages trigger an orange overlay banner for 3 seconds showing sender and preview.

### Keyboard Features
- `SFT` — toggle shift (auto-releases after one character)
- `NUM` — toggle number/symbol layer
- `<<` — backspace
- `SND` — broadcast message to all peers

---

## Serial Debug Output

Open Serial Monitor at **115200 baud** to see:
- This device's MAC address (copy this to add as a named peer on other units)
- Incoming packet log
- Peer discovery events
- Send results

---

## Limitations & Known Issues

- **No encryption** — ESP-NOW packets are unencrypted by default.  
  For private messages, add `peer.encrypt = true` and set an LMK.
- **No persistent storage** — messages are lost on reboot (no SPIFFS/NVS yet).
- **250-byte ESP-NOW limit** — message bodies capped at 200 chars.
- **Broadcast ACKs** — the current implementation sends ACKs for all received messages;  
  for true delivery confirmation you'd need directed (unicast) sends.

---

## Potential Enhancements

- [ ] NVS/SPIFFS message persistence across reboots
- [ ] Encrypted unicast to a selected peer (from Peers screen)
- [ ] Longer messages via multi-packet fragmentation
- [ ] RTC module for real timestamps
- [ ] Sound/haptic notification on the CYD buzzer (GPIO 26)

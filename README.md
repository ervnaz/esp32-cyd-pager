# ESP32 CYD Pager

A standalone off-grid mesh pager using **ESP-NOW** on the **ESP32-2432S028R** ("Cheap Yellow Display"). Devices communicate directly with each other — no WiFi router, no internet, no cloud.

Two flavours are provided:
- **Arduino sketch** (`esp32_pager.ino`) — full touchscreen UI with on-screen keyboard, runs standalone with no Home Assistant dependency
- **ESPHome** (`pager-node.yaml`) — integrates with Home Assistant, supports OTA updates and HA automations

---

## Hardware

| | |
|---|---|
| Board | ESP32-2432S028R (CYD — Cheap Yellow Display) |
| Display | 2.8" ILI9341 TFT 320×240 |
| Touch | XPT2046 resistive |
| Radio | ESP-NOW over 2.4 GHz WiFi radio |
| Range | ~100 m open air |

> **Important:** This board has two hardware variants with different SPI pin mappings. Check which variant you have before flashing — see Wiring Reference below.

---

## Features

- **Mesh messaging** — broadcast messages to all peers in range simultaneously
- **Auto peer discovery** — devices find each other automatically via heartbeat beacons, no manual MAC entry
- **On-screen keyboard** — uppercase/lowercase with SHIFT toggle, DEL, SPACE, SEND
- **Message history** — last 5 received messages shown in INBOX with relative timestamps
- **Notification banner** — orange overlay appears for 3 seconds on incoming message
- **Unread badge** — `[N]` counter in status bar, auto-clears when INBOX is opened
- **Three screens** — INBOX, COMPOSE, PEERS, switchable via bottom tab bar
- **Home Assistant integration** (ESPHome version) — send messages via HA service, trigger automations on receive
- **OTA updates** (ESPHome version) — update firmware over WiFi

---

## File Structure

```
esp32-cyd-pager/
│
├── Arduino sketch
│   ├── esp32_pager.ino      ← Main sketch (open in Arduino IDE)
│   ├── config.h             ← Pins, timing, colours — edit per unit
│   ├── message_types.h      ← ESP-NOW wire format structs
│   ├── message_store.h      ← Inbox ring-buffer (30 messages)
│   ├── espnow_manager.h     ← ESP-NOW send/receive/peer management
│   ├── ui.h                 ← UI public API
│   ├── ui.cpp               ← All screen drawing & touch handling
│   └── User_Setup.h         ← TFT_eSPI config → copy to library folder
│
├── ESPHome
│   ├── pager-node.yaml      ← ESPHome config (one per unit)
│   ├── espnow_bridge.h      ← Custom ESP-NOW C++ bridge for ESPHome
│   └── secrets.yaml         ← WiFi / API / OTA credentials (not committed)
│
├── CI
│   ├── platformio.ini       ← PlatformIO build config
│   └── .github/workflows/
│       └── build.yml        ← GitHub Actions CI
│
└── README.md
```

---

## Wiring Reference

The CYD has all components soldered on-board — no external wiring needed. However there are two known variants with different display SPI pins:

### Variant A — Standard CYD (most common)
| Function | GPIO |
|---|---|
| TFT CLK | 18 |
| TFT MOSI | 23 |
| TFT MISO | 19 |
| TFT CS | 15 |
| TFT DC | 2 |
| TFT BL | 21 |
| Touch CLK | 25 |
| Touch MOSI | 32 |
| Touch MISO | 39 |
| Touch CS | 33 |
| Touch IRQ | 36 |

### Variant B — Alternative pinout (some units)
| Function | GPIO |
|---|---|
| TFT CLK | **14** |
| TFT MOSI | **13** |
| TFT MISO | **12** |
| TFT CS | 15 |
| TFT DC | 2 |
| TFT BL | 21 |
| Touch CLK | 25 |
| Touch MOSI | 32 |
| Touch MISO | 39 |
| Touch CS | 33 |
| Touch IRQ | 36 |

> If the screen stays white after flashing, try switching between Variant A and B pin mappings.

---

## Quickstart — Arduino

### 1. Install libraries

| Library | Install via |
|---|---|
| TFT_eSPI by Bodmer | Arduino Library Manager |
| XPT2046_Touchscreen by Paul Stoffregen | Arduino Library Manager |
| esp_now | Built into ESP32 Arduino core |
| WiFi | Built into ESP32 Arduino core |

### 2. Configure TFT_eSPI

Copy `User_Setup.h` into your TFT_eSPI library folder, replacing the existing file:

```
Arduino/libraries/TFT_eSPI/User_Setup.h
```

### 3. Set device name

Edit `config.h` before flashing each unit — every device needs a unique name:

```cpp
#define DEVICE_NAME   "UNIT-1"   // max 8 chars
```

### 4. Flash

Open `esp32_pager.ino` in Arduino IDE, select **ESP32 Dev Module**, and upload.

### 5. Or use PlatformIO

```bash
# Compile and flash (resistive touch)
pio run -e cyd --target upload

# Capacitive touch variant
pio run -e cyd_cap --target upload
```

---

## Quickstart — ESPHome

### 1. Copy files

Place these files in your ESPHome config directory (e.g. `/config/esphome/`):

```
pager-node.yaml
espnow_bridge.h
secrets.yaml       ← fill in your credentials
```

> **Important:** Upload `espnow_bridge.h` using the **File Editor addon** or SSH — do not paste it through the ESPHome web editor as the `#` characters in preprocessor directives may be stripped.

### 2. Create secrets.yaml

```yaml
wifi_ssid: "YourWiFiSSID"
wifi_password: "YourWiFiPassword"
ap_password: "pager1234"
ota_password: "your-ota-password"
api_encryption_key: "your-base64-32-byte-key"  # openssl rand -base64 32
```

### 3. Set device name

In `pager-node.yaml`, change the substitution at the top for each unit:

```yaml
substitutions:
  device_name: "unit-1"      # ← unique per unit
  device_friendly: "Pager Unit 1"
```

### 4. Flash

```bash
# First flash via USB
esphome run pager-node.yaml

# Subsequent updates via OTA
esphome run pager-node.yaml
```

### 5. Send messages from Home Assistant

Once connected to HA, use the Developer Tools service call:

```yaml
service: esphome.cyd_pager_unit_1_send_message
data:
  message: "Hello from HA!"
```

---

## UI Overview

### Status bar (always visible)
| Position | Content |
|---|---|
| Left | Device name |
| Centre | Active peer count |
| Right | Unread message count `[N]` (orange) |

### Tab bar (bottom, tap to switch)
| Tab | Description |
|---|---|
| INBOX | Last 5 received messages with sender and relative timestamp. Tapping clears unread count. |
| COMPOSE | On-screen keyboard. Type a message and tap SEND to broadcast to all peers. |
| PEERS | Active peer count and last heard sender. |

### Keyboard
| Key | Action |
|---|---|
| Letter keys | Type character (lowercase by default) |
| SHF | Toggle shift — highlighted yellow when active, auto-releases after one character |
| SPC | Space |
| DEL | Backspace |
| SEND | Broadcast message to all peers, return to INBOX |

### Notification banner
Incoming messages trigger an orange overlay banner for 3 seconds showing the sender and message preview.

---

## ESP-NOW Channel

All devices **must use the same WiFi channel**.

- **Arduino version:** Set `ESPNOW_CHANNEL` in `config.h` (default: 1). All units must match.
- **ESPHome version:** Channel is set automatically to match the connected WiFi router channel. All ESPHome units connecting to the same router will automatically use the same channel.

> If mixing Arduino and ESPHome units in the same mesh, ensure the Arduino units are configured to use the same channel as your WiFi router.

---

## Touch Calibration

The default calibration values in `config.h` / `pager-node.yaml` are approximate:

```
X_MIN: 200   X_MAX: 3800
Y_MIN: 300   Y_MAX: 3700
```

To calibrate for your specific board, run the **TFT_eSPI Touch_calibrate** example sketch and paste the output values into your config.

---

## Known Limitations

| Limitation | Detail |
|---|---|
| No encryption | ESP-NOW packets are unencrypted by default. Add `peer.encrypt = true` and set an LMK for private comms. |
| No persistence | Messages are lost on reboot (NVS storage planned). |
| 200 char limit | ESP-NOW packets are capped at 250 bytes; message bodies capped at 200 chars. |
| Broadcast only | All messages go to all peers. Direct unicast to a specific peer is planned. |
| No RTC | Timestamps are relative to uptime, not wall-clock time. Add an RTC module for real timestamps. |

---

## Planned Enhancements

- [ ] Peer list in PEERS tab with individual online/offline status
- [ ] Numbers and symbols keyboard layer
- [ ] ACK confirmation tick on sent messages
- [ ] Buzzer notification on GPIO 26
- [ ] NVS message persistence across reboots
- [ ] Direct (unicast) messaging to selected peer
- [ ] Home Assistant automation triggers on message received
- [ ] Longer messages via multi-packet fragmentation

---

## Contributing

Pull requests welcome. Please test on hardware before submitting — the CI pipeline only verifies compilation.

```bash
# Run CI compile check locally
pio run -e ci
```

---

## License

MIT

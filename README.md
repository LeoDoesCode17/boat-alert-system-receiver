# Boat Alert System — Receiver

Ground-side receiver for the **Boat Alert System**. It listens for encrypted LoRa
telemetry packets sent by the **Boat Alert System Transmitter** (mounted on the
boat), authenticates and decodes the boat's orientation (roll / pitch / yaw),
GPS position and danger state, and surfaces that data locally via an OLED
display with rotary-encoder navigation and buzzer/LED alerting. The receiver
can also join a Wi-Fi network and connect to an MQTT broker, with credentials
set from a phone through a built-in configuration web page.

## Overview

The transmitter unit on the boat periodically sends an AES-128-GCM encrypted
LoRa packet containing its IMU orientation, GPS fix and danger flag. This
receiver:

1. Listens for incoming LoRa packets over SX1278 (433 MHz).
2. Validates the frame, then decrypts and authenticates it (AES-128-GCM).
3. Parses the plaintext into structured telemetry.
4. Displays the latest telemetry on a 0.96" I2C OLED.
5. Lets the operator navigate between display pages using a rotary encoder.
6. Flags loss of signal / loss of GPS fix via LEDs and a buzzer.
7. Connects to Wi-Fi and an MQTT broker (configured through a web page served
   by the ESP32 itself) and reports the connection status on screen.

## Hardware

| Component                | Notes                            |
|--------------------------|----------------------------------|
| ESP32 Dev Module         | Main controller                  |
| LoRa SX1278 (433 MHz)    | Receives telemetry from the boat |
| OLED Display 0.96" (I2C) | SSD1306, 128x64                  |
| Rotary Encoder (KY-040)  | Menu/page navigation             |
| Buzzer                   | Audible alerting                 |
| LED (Red)                | Alert / signal-lost indicator    |
| LED (Green)              | Normal / live-fix indicator      |

### Pin Configuration

**LoRa SX1278**

| Signal | ESP32 GPIO |
|--------|------------|
| RST    | 14         |
| DIO0   | 26         |
| NSS    | 5          |
| MOSI   | 23         |
| MISO   | 19         |
| SCK    | 18         |

**Rotary Encoder (KY-040)**

| Signal | ESP32 GPIO |
|--------|------------|
| DT     | 33         |
| SW     | 4          |
| CLK    | 32         |

**Outputs**

| Signal      | ESP32 GPIO |
|-------------|------------|
| Buzzer      | 13         |
| LED (Red)   | 25         |
| LED (Green) | 27         |

**OLED (I2C)** uses the ESP32's standard I2C bus (`SDA_PIN` / `SCL_PIN` as
defined in `config/pin.h`), address `0x3C`.

## Telemetry Packet Format

### Over the air (encrypted frame)

Every LoRa packet is a single AES-128-GCM frame:

| Field      | Size (bytes) | Notes                                              |
|------------|--------------|----------------------------------------------------|
| AAD        | 2            | Authenticated header (not encrypted)               |
| Nonce      | 12           | GCM nonce                                          |
| Length     | 1            | Ciphertext length `N` (1–224)                      |
| Ciphertext | N            | Encrypted telemetry text                           |
| Tag        | 16           | GCM authentication tag                             |

The total packet size must equal `31 + N`. Frames with a wrong size or length
field are dropped, and frames whose tag does not verify are discarded as
unauthenticated. The receiver keeps counters for received, malformed,
authentication-failed and parse-failed packets (`lora::getStats()`).

### Decrypted payload

After decryption the payload is a comma-separated, key:value ASCII string:

```
R:2.13,P:-1.05,Y:184.30,LAT:-5.123456,LON:120.123456,FIX:1,S:0
```

| Key | Meaning                                                                |
|-----|------------------------------------------------------------------------|
| R   | Roll (degrees)                                                         |
| P   | Pitch (degrees)                                                        |
| Y   | Yaw (degrees)                                                          |
| LAT | Latitude                                                               |
| LON | Longitude                                                              |
| FIX | `1` = coordinates from a live GPS fix, `0` = fallback/default location |
| S   | `1` = danger state reported by the transmitter, `0` = safe             |

The receiver only accepts a packet as valid once all seven fields have been
parsed; a malformed or truncated payload is logged and discarded rather than
partially applied.

## Firmware Architecture

Built with **PlatformIO** + **Arduino framework** on **FreeRTOS**, organized
into managers so each subsystem stays independently testable and swappable.
Headers and sources are grouped by folder:

```
main.cpp                   # Task creation: network task (core 0), main task (core 1)
config/
  pin.h                    # Centralized pin definitions
core/
  type.h                   # WifiConfig / MqttConfig structs
managers/
  page_manager             # Page navigation + per-loop UI dispatch
  sensor_manager           # Rotary encoder / onboard sensors
  network_manager          # Facade over LoRa, Wi-Fi, MQTT and the config access point
  nvs_manager              # Persists Wi-Fi / MQTT credentials in flash (NVS)
networks/
  lora                     # SX1278 init, frame validation, receive + parse
pages/
  i_page.h, page_id.h      # Page interface and page identifiers
  landing_page, menu_page, live_monitoring_page, template_page,
  network_configuration_page, configuration_mode_page,
  network_status_page, ...
  ui_widgets               # Shared header / button / text helpers
security/
  aes                      # AES-128-GCM decrypt + authentication
sensors/
  rotary_encoder           # KY-040 driver (ESP32Encoder + debounced button)
```

- **`networks/lora`** owns the SX1278 radio directly: initialization, SPI pin
  remapping, non-blocking packet polling, frame-size validation, decryption
  through `security/aes`, and parsing into a `TelemetryPacket` struct.
- **`managers/network_manager`** is the single entry point the rest of the
  firmware uses for networking. It proxies LoRa, runs the Wi-Fi/MQTT
  connect-and-reconnect logic, and hosts the configuration access point.
- **`managers/nvs_manager`** loads saved credentials at boot and stores new
  ones when the configuration form is submitted.
- **`managers/page_manager`** drives the OLED UI. Every screen implements the
  `IPage` interface (`onEnter` / `update` / `onExit`), so adding a page means
  adding a class, a `PageId` and one `case` in the page manager.

### FreeRTOS tasks

| Task        | Core | Responsibilities                                                  |
|-------------|------|-------------------------------------------------------------------|
| `MainTask`    | 1    | LoRa receive, configuration web server, navigation and UI updates |
| `NetworkTask` | 0    | Wi-Fi and MQTT connect / reconnect (publish/receive to follow)    |

Rules that keep the two cores safe:

- LoRa is only ever driven from core 1 (`update_on_core_1()`).
- Wi-Fi/MQTT only run on core 0 (`update_on_core_0()`), so a blocking MQTT
  connect never freezes the UI or the radio.
- Credentials shared between cores are guarded by a mutex; connection status
  flags are atomics, so the UI can read them from any task.
- While the configuration access point is on, Wi-Fi/MQTT are suspended and
  resume automatically when you leave configuration mode.

### Data flow

```
LoRa packet → networks/lora (validate → decrypt → parse) → managers/network_manager
            → MainTask → managers/page_manager (display) / alert logic (LED, buzzer)

NetworkTask → Wi-Fi + MQTT connect/reconnect → status flags → Network Status page
```

## User Interface

The 128x64 OLED is navigated with the rotary encoder: **rotate** to move,
**press** to select. Sub-pages end with a highlighted back button that
returns to the parent page.

| Page                  | Purpose                                                                          |
|-----------------------|----------------------------------------------------------------------------------|
| Landing               | Splash screen; press to open the menu                                            |
| Menu                  | Carousel of pages (wraps around in both directions)                              |
| Live Monitoring       | Roll / pitch / yaw, GPS fix, latitude / longitude, RSSI, SAFE / DANGER, `NO SIGNAL` |
| Network Configuration | Shows the saved Wi-Fi SSID and MQTT host; **Configure** starts the access point  |
| Configuration Mode    | Shows the access point name, password and IP; **Cancel** / **Back** stops it     |
| Network Status        | Wi-Fi and MQTT connection state plus the IP address                              |
| Settings              | Settings page that Network Status returns to                                     |
| Template              | Placeholder page to copy when adding new screens                                 |

## Wi-Fi & MQTT Setup

Credentials are entered from a phone or laptop; no hard-coding or re-flashing
is needed.

1. On the device open **Menu → Network Configuration** and select **Configure**.
2. The **Configuration Mode** page shows the access point name, password and
   IP address. Join that Wi-Fi network from your phone.
3. Open the displayed IP (default `192.168.4.1`) in a browser.
4. Fill in the Wi-Fi SSID / password and the MQTT broker host, port and
   optional username / password, then press **Save**.
5. The screen shows "Credentials saved!". Press **Back**: the access point
   shuts down and the receiver connects with the new settings.

Credentials are stored in flash (NVS) and reused at the next boot. After that
the network task keeps the connection alive: failed Wi-Fi or MQTT attempts are
retried with an increasing delay (2 s up to 30 s), and dropped connections are
re-established automatically. Use the **Network Status** page to check the
result.

> The access point name and password are defined in `managers/network_manager.cpp`.
> Change the default password before deploying.

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE extension)
- USB connection to the ESP32 Dev Module

### Dependencies

Declared in `platformio.ini`:

```ini
lib_deps =
    sandeepmistry/LoRa@^0.8.0
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SSD1306
    madhephaestus/ESP32Encoder
    knolleary/PubSubClient@^2.8
```

### Build & Flash

```bash
pio run                # build
pio run -t upload      # flash to the ESP32
pio device monitor     # view serial output (115200 baud)
```

### LoRa Radio Settings

The receiver uses the LoRa library defaults: spreading factor 7, 125 kHz
bandwidth, coding rate 4/5 and sync word `0x12`. These **must match the
transmitter's settings exactly** for packets to decode. If the transmitter uses
non-default values, update them in `networks/lora.cpp` inside `initialize()`.

### Encryption Key

Packets are encrypted with AES-128-GCM, so the receiver and the transmitter
must share the same key and the same frame layout (see
[Telemetry Packet Format](#telemetry-packet-format)). Key handling lives in
`security/aes`; never commit a production key to the repository.

## Status / Alerting Logic

- **Green LED** — telemetry is fresh and `FIX:1` (live GPS fix).
- **Red LED / buzzer** — signal lost (no packet within the configured
  timeout) or `FIX:0` (boat is reporting a fallback location, not a live
  fix).
- The **Live Monitoring** page shows the transmitter's danger flag (`S`) as a
  highlighted `STATUS: DANGER` bar and shows `NO SIGNAL` in the header when no
  packet has arrived within the timeout.

## Roadmap

- [x] Encrypted telemetry (AES-128-GCM) with frame validation
- [x] Wi-Fi connectivity with automatic reconnect
- [x] MQTT broker connectivity with automatic reconnect
- [x] Wi-Fi / MQTT configuration through an on-device web page, stored in NVS
- [x] Network status page (Wi-Fi, MQTT, IP)
- [ ] MQTT publishing of received telemetry
- [ ] Replay protection and header (version / device ID) checks on received frames
- [ ] Configurable alert thresholds via rotary encoder menu
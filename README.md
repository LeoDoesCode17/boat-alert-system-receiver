# Boat Alert System — Receiver

Ground-side receiver for the **Boat Alert System**. It listens for LoRa telemetry
packets sent by the **Boat Alert System Transmitter** (mounted on the boat),
decodes the boat's orientation (roll / pitch / yaw) and GPS position, and
surfaces that data locally via an OLED display, with rotary-encoder input and
buzzer/LED alerting.

## Overview

The transmitter unit on the boat periodically sends a LoRa packet containing
its IMU orientation and GPS fix. This receiver:

1. Listens for incoming LoRa packets over SX1278 (433 MHz).
2. Parses the packet into structured telemetry.
3. Displays the latest telemetry on a 0.96" I2C OLED.
4. Lets the operator navigate between display pages using a rotary encoder.
5. Flags loss of signal / loss of GPS fix via LEDs and a buzzer.

## Hardware

| Component                  | Notes                          |
|-----------------------------|---------------------------------|
| ESP32 Dev Module            | Main controller                |
| LoRa SX1278 (433 MHz)       | Receives telemetry from the boat |
| OLED Display 0.96" (I2C)    | SSD1306, 128x64                |
| Rotary Encoder (KY-040)     | Menu/page navigation            |
| Buzzer                      | Audible alerting                |
| LED (Red)                   | Alert / signal-lost indicator   |
| LED (Green)                 | Normal / live-fix indicator     |

### Pin Configuration

**LoRa SX1278**

| Signal | ESP32 GPIO |
|--------|-----------|
| RST    | 14        |
| DIO0   | 26        |
| NSS    | 5         |
| MOSI   | 23        |
| MISO   | 19        |
| SCK    | 18        |

**Rotary Encoder (KY-040)**

| Signal | ESP32 GPIO |
|--------|-----------|
| DT     | 33        |
| SW     | 4         |
| CLK    | 32        |

**Outputs**

| Signal      | ESP32 GPIO |
|-------------|-----------|
| Buzzer      | 13        |
| LED (Red)   | 25        |
| LED (Green) | 27        |

**OLED (I2C)** uses the ESP32's standard I2C bus (`SDA_PIN` / `SCL_PIN` as
defined in `config/pin.h`), address `0x3C`.

## Telemetry Packet Format

The transmitter sends telemetry as a comma-separated, key:value ASCII string:

```
R:2.13,P:-1.05,Y:184.30,LAT:-5.123456,LON:120.123456,FIX:1
```

| Key | Meaning                                                    |
|-----|-------------------------------------------------------------|
| R   | Roll (degrees)                                              |
| P   | Pitch (degrees)                                             |
| Y   | Yaw (degrees)                                               |
| LAT | Latitude                                                     |
| LON | Longitude                                                    |
| FIX | `1` = coordinates from a live GPS fix, `0` = fallback/default location |

The receiver only accepts a packet as valid once all six fields have been
parsed; a malformed or truncated packet is logged and discarded rather than
partially applied.

## Firmware Architecture

Built with **PlatformIO** + **Arduino framework**, organized into managers so
each subsystem stays independently testable and swappable:

```
include/
  config/
    pin.h                  # Centralized pin definitions
  managers/
    page_manager.h
    sensor_manager.h
    network_manager.h      # Facade over all network subsystems (LoRa now, WiFi/MQTT later)
  networks/
    lora.h                 # LoRa radio init, packet receive + parse

src/
  main.cpp
  managers/
    page_manager.cpp
    sensor_manager.cpp
    network_manager.cpp
  networks/
    lora.cpp
```

- **`networks/lora`** owns the SX1278 radio directly: initialization, SPI pin
  remapping, non-blocking packet polling, and parsing into a
  `TelemetryPacket` struct.
- **`managers/network_manager`** is the single entry point the rest of the
  firmware talks to for all networking — currently proxies to `lora`, and
  will grow to also own WiFi and MQTT without changing any calling code.
- **`managers/page_manager`** and **`managers/sensor_manager`** handle the
  OLED UI and onboard sensors respectively, and consume telemetry only
  through `network_manager`.

### Data flow

```
LoRa packet → networks/lora (parse) → managers/network_manager (facade)
            → main.cpp loop → managers/page_manager (display) / alert logic (LED, buzzer)
```

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
```

### Build & Flash

```bash
pio run                # build
pio run -t upload      # flash to the ESP32
pio device monitor      # view serial output (115200 baud)
```

### LoRa Radio Settings

The receiver currently uses default LoRa radio parameters (spreading factor,
bandwidth, coding rate, sync word). These **must match the transmitter's
settings exactly** for packets to decode. If the transmitter uses non-default
values, update them in `networks/lora.cpp` inside `initialize()`.

## Status / Alerting Logic

- **Green LED** — telemetry is fresh and `FIX:1` (live GPS fix).
- **Red LED / buzzer** — signal lost (no packet within the configured
  timeout) or `FIX:0` (boat is reporting a fallback location, not a live
  fix).

## Roadmap

- [ ] WiFi connectivity (`networks/wifi`)
- [ ] MQTT publishing of received telemetry (`networks/mqtt`)
- [ ] Configurable alert thresholds via rotary encoder menu
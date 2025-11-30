# Viessmann-Waermepumpe-in-ESPhome-with-Optolink-DIY
[![Release](https://github.com/MomoRC-tech/Viessmann-Waermepumpe-in-ESPhome-with-Optolink-DIY/actions/workflows/release.yml/badge.svg)](https://github.com/MomoRC-tech/Viessmann-Waermepumpe-in-ESPhome-with-Optolink-DIY/actions/workflows/release.yml)
Connect a Viessmann heat pump via DIY Optolink to Home Assistant (MQTT) using VitoWiFi v3, grouped polling, WebSerial, and ElegantOTA.

## Project overview

This project uses an ESP8266 D1 mini (Arduino framework) to connect a DIY Optolink adapter to a Viessmann Vitocal 343-G heat pump. Communication to the heat pump is handled by the VitoWiFi v3 library over SoftwareSerial. Home Assistant integration is done via MQTT using ArduinoHA entities defined in `HA_mqtt_addin.h`.

The codebase was migrated from older VitoWiFi v2-style datapoints to VitoWiFi v3. Datapoints and polling are grouped, with adjustable intervals from Home Assistant. OTA updates and a lightweight debug console are provided via ElegantOTA and WebSerial on an async web server.

### Hardware and Wiring

- **Microcontroller:** ESP8266 D1 mini
- **Optolink link (SoftwareSerial):**
  - RX: `D1` (GPIO5)
  - TX: `D2` (GPIO4)
- **Optolink adapter:** DIY design based on the openv project
	- Base schematic: https://github.com/openv/openv/wiki/Bauanleitung-ESP8266
	- IR send diode: `L-7104SF4BT` (replaces the diode from the reference design)

Note: The ESP8266 RX pin uses an internal pull-up in code to improve signal stability at 4800 baud 8E1.

The schematic largely follows the openv ESP8266 instructions and typical Optolink transceiver wiring. Ensure the IR transceiver is aligned properly on the Viessmann service port.

### Software and Libraries

- **Platform:** Arduino (ESP8266 core) on a D1 mini
- **Viessmann communication:** `VitoWiFi v3` using the Viessmann “KW” protocol (named “VS1” within VitoWiFi)
- **Home Assistant integration:** MQTT via `ArduinoHA`
- **OTA and web:** `ESP Async WebServer` + `ElegantOTA` (async mode) + `WebSerial`

Notes:
- VitoWiFi v3 renamed and regrouped various datapoints versus v2. This repo uses v3-style datapoints declared in `Vitocal_datapoints.h`.
- We use SoftwareSerial on ESP8266 at 4800 baud, 8E1 (`SWSERIAL_8E1`).
- Operation and manual mode labels are localized in German (see `HA_mqtt_addin.h`).

### Features

- Reliable two-way communication with the Viessmann Vitocal 343-G via Optolink using VitoWiFi v3 (protocol “VS1”/KW).
- Grouped polling scheduler with HA-adjustable intervals (fast/medium/slow) defined in `Vitocal_polling.h` and exposed in HA via `HA_mqtt_addin.h`.
- Home Assistant entities (numbers/selects/switches) bound to datapoints and commands.
- Web UI on the ESP8266 providing ElegantOTA and a WebSerial console for debugging.
- German labels for operation modes and manual modes restored for UI consistency.

### Async WebServer, ElegantOTA, and WebSerial
- The project uses `ESP Async WebServer` to avoid blocking the main loop and to serve ElegantOTA and WebSerial endpoints concurrently.
- `ElegantOTA` in async mode requires defining the async build flag (e.g., `ELEGANTOTA_USE_ASYNC_WEBSERVER=1`) so its handlers link against the async stack.
- `WebSerial` provides a browser-based serial console for quick diagnostics; VitoWiFi and app logs can be viewed without a physical USB serial connection.

### Protocol
- Viessmann “KW” protocol is used for communication; in VitoWiFi this protocol is named “VS1”. Ensure VitoWiFi is configured to operate with VS1 when building.

### Build and CI
- Local builds use Arduino CLI with the necessary compiler flags for ElegantOTA async.
- CI is provided via GitHub Actions in `.github/workflows/ci.yml` to verify builds against VitoWiFi v3 and required libraries.

### Security
- Avoid hardcoding credentials in source. This repo uses a `secrets.h` (git-ignored) that defines `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_USER`, and `MQTT_PASS`. Update `secrets.h` locally with your values.
- Alternatively, consider WiFiManager or runtime configuration for production.
 - A `secrets.example.h` is provided. Copy it to `secrets.h` and set your credentials. The real `secrets.h` is ignored by git.
 - Fallback logic: If `secrets.h` is absent, the sketch automatically includes `secrets.example.h` so CI builds succeed without real credentials.

### Diagnostics
- Home Assistant entities for device health:
	- `vito_error_count`: total errors within a rolling window.
	- `vito_consecutive_errors`: current consecutive error streak.
	- `vito_error_threshold`: configurable consecutive error threshold (default 30; range 1–100).
- When the threshold is reached, the firmware applies a brief backoff (increases poll intervals) and reinitializes VitoWiFi.

### Key Files
- `Vitocal_basic-esp8266-Bartels.ino`: Main sketch, async web server setup, VitoWiFi init, OTA/WebSerial, and polling loop.
- `Vitocal_datapoints.h`: VitoWiFi v3 datapoints and access helpers.
- `Vitocal_polling.h`: Grouped polling state and intervals.
- `HA_mqtt_addin.h`: Home Assistant MQTT entities and callbacks.

### Folder Layout
All Arduino sources now reside in the directory `Vitocal_basic-esp8266-Bartels/` (sketch + headers). Workflows (`ci.yml`, `release.yml`) were updated to compile and package from this folder. Update any local Arduino IDE sketch references accordingly.

### Useful Links
- Schematic (generic): https://github.com/openv/openv/wiki/ESPHome-Optolink
- 3D housing (Wemos D1 mini enclosure): https://makerworld.com/de/models/1567595-viessmann-optolink-esp8266-wemos-d1-mini-enclosure#profileId-1648098
- Related documentation:
	- https://github.com/openv/openv/wiki/Bauanleitung-ESP8266
	- https://github.com/openv/openv/wiki/Bauanleitung-LAN-Ethernet
- VitoWiFi project: https://github.com/bertmelis/VitoWiFi

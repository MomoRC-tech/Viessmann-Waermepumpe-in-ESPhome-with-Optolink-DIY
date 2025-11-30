# Viessmann-Waermepumpe-in-ESPhome-with-Optolink-DIY
Connect Viessmann Waermepumpe to Home Assistant as ESPhome device with Optolink DIY

## Project overview

This project uses an ESP8266 D1 mini (Arduino framework) to connect a DIY Optolink adapter to a Viessmann Vitocal 343-G heat pump. The ESP8266 reads and writes data over a software serial interface and exposes it to Home Assistant via ESPHome.

### Hardware and wiring

- **Microcontroller:** ESP8266 D1 mini
- **Communication to Optolink:** SoftwareSerial
	- `D1` → `GPIO5` (SoftwareSerial RX)
	- `D2` → `GPIO4` (SoftwareSerial TX)
- **Optolink adapter:** DIY design based on the openv project
	- Base schematic: https://github.com/openv/openv/wiki/Bauanleitung-ESP8266
	- IR send diode: `L-7104SF4BT` (replaces the diode from the reference design)

The schematic largely follows the openv ESP8266 build instructions, with the above IR diode substitution and wiring of the ESP8266 D1 mini pins D1/D2 for the SoftwareSerial connection to the Optolink transceiver.

### Software and libraries

- **Platform:** Arduino (ESP8266 core) on a D1 mini
- **Integration:** ESPHome with MQTT connection to Home Assistant running on a Raspberry Pi 4 (and optionally the ESPHome native API)
- **Viessmann communication:** `vitowifi` library (latest version)
	- Note: the `vitowifi` API changed between versions `v2.x` and `v3.x`. This project is designed for the latest `vitowifi` (3.x) API, so be careful when looking at older examples or sketches that still use the 2.x API.

The main goals are:

- Establish reliable two-way communication with the Viessmann Vitocal 343-G via the DIY Optolink adapter.
- Use the `vitowifi` library to read sensor values and control selected parameters of the heat pump.
- Make these values and controls available in Home Assistant through ESPHome.

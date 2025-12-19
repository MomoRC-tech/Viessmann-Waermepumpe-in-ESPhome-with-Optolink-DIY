# Changelog

All notable changes to this project will be documented here.

## [v0.3.1] - 2025-12-19
- Home Assistant: publish initial states for polling interval Number entities on MQTT connect (fixes empty/unknown values)
- Also publishes error threshold Number state on connect

## [v0.3.0] - 2025-12-19
- Home Assistant: update HA entity config

## [v0.2.9] - 2025-12-04
- ESP32-C3 test: increase deferred read/retry interval to 1s for stability
- Tag triggers CI compile for both ESP8266 main and ESP32-C3 test

## [v0.2.8] - 2025-12-04
- ESP32-C3 test: add deferred scheduling and loop-based retry for VitoWiFi read chaining (200ms min)
- Improves handling of queue-busy/timeouts by pacing requests

## [v0.2.5] - 2025-11-30
- Version constant aligned with release tag (DEVICE_SWVERSION 0.2.5)
- No functional code changes beyond version bump

## [v0.2.4] - 2025-11-30
- Consolidated ElegantOTA async configuration (single guarded define in sketch)
- Removed redundant CI compile flag for async OTA mode
- Added fallback `secrets.example.h` with prominent compile-time warnings
- Updated `DEVICE_SWVERSION` constant

## [v0.2.3] - 2025-11-30
- Removed legacy/placeholder duplicate VitoWiFi datapoints
- Minor documentation improvements

## [v0.2.2] - 2025-11-29
- Restructured repository into dedicated sketch folder `Vitocal_basic-esp8266-Bartels/`
- Updated CI and release workflows to new file paths
- Confirmed release packaging (sketch folder zip)

## Earlier
- Initial project setup and basic datapoint polling (pre-tag)

### Notes
- Real secrets provided via untracked `secrets.h`; builds fall back to example file if absent
- To disable async OTA explicitly, define `ELEGANTOTA_USE_ASYNC_WEBSERVER 0` before including `ElegantOTA.h`

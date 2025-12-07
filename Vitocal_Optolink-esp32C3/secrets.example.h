#pragma once

// Copy this file to `secrets.h` and fill in your local credentials.
// `secrets.h` is git-ignored to keep secrets out of the repo.
//
// VERY VISIBLE WARNING: If you see this during compilation you are
// building with placeholder credentials. WiFi and MQTT will NOT
// authenticate successfully until you provide real values in
// `secrets.h`.
#ifdef __GNUC__
#warning "Using placeholder credentials from secrets.example.h – provide real secrets in secrets.h"
#endif
#pragma message ("Using placeholder credentials from secrets.example.h – provide real secrets in secrets.h")

static const char* WIFI_SSID     = "YOUR_WIFI_SSID";
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
static const char* MQTT_USER     = "YOUR_MQTT_USERNAME";
static const char* MQTT_PASS     = "YOUR_MQTT_PASSWORD";

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#ifndef ELEGANTOTA_USE_ASYNC_WEBSERVER
#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#endif
#include <ElegantOTA.h>
#include <VitoWiFi.h>

// ----------------------------------------------------------------------------
// HARDWARE CONFIGURATION: ESP32-C3 Super Mini
// ----------------------------------------------------------------------------
// The ESP32-C3 Super Mini uses native USB (CDC) for the "Serial" object.
// We use the Hardware UART0 for the Optolink.
//
// WIRING (ESP32-C3 Super Mini):
// - GPIO 20: RX (Connect to Optolink TX/Output)
// - GPIO 21: TX (Connect to Optolink RX/Input)
// - 3.3V / GND
// ----------------------------------------------------------------------------

#define OPTOLINK_SERIAL Serial0
#define CONSOLE_SERIAL  Serial
#define SERIALBAUDRATE  115200

// WiFi credentials: prefer local secrets.h, else fallback example
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// Async web server for ElegantOTA
AsyncWebServer server(80);

// Initialize VitoWiFi with the hardware serial port
VitoWiFi::VitoWiFi<VitoWiFi::VS1> vitoWiFi(&OPTOLINK_SERIAL);

// ----------------------------------------------------------------------------
// DATAPOINTS
// ----------------------------------------------------------------------------
VitoWiFi::Datapoint datapoints[] = {
  VitoWiFi::Datapoint("outsidetemp", 0x0101, 2, VitoWiFi::div10),
  VitoWiFi::Datapoint("boilertemp",  0x010D, 2, VitoWiFi::div10),
  VitoWiFi::Datapoint("pump",        0x048D, 1, VitoWiFi::noconv)
};

// Calculate number of datapoints automatically
const uint8_t NUM_DATAPOINTS = sizeof(datapoints) / sizeof(VitoWiFi::Datapoint);
uint8_t currentDatapointIndex = 0;
bool isReadingSequence = false;

// Forward declarations
void readNextDatapoint();

// ----------------------------------------------------------------------------
// CALLBACKS
// ----------------------------------------------------------------------------

void onResponse(const uint8_t* data, uint8_t length, const VitoWiFi::Datapoint& request) {
  CONSOLE_SERIAL.printf("[%s] Raw: ", request.name());
  for (uint8_t i = 0; i < length; ++i) {
    CONSOLE_SERIAL.printf("%02X ", data[i]);
  }

  CONSOLE_SERIAL.print(" -> Decoded: ");

  if (request.converter() == VitoWiFi::div10) {
    float value = request.decode(data, length);
    CONSOLE_SERIAL.printf("%.1f C\n", value);
  } 
  else if (request.converter() == VitoWiFi::noconv) {
    // Assuming simple ON/OFF for pump status
    CONSOLE_SERIAL.printf("%s\n", (data[0] > 0) ? "ON" : "OFF");
  }

  // DAISY CHAIN: Successfully received data, now request the next one
  readNextDatapoint();
}

void onError(VitoWiFi::OptolinkResult error, const VitoWiFi::Datapoint& request) {
  CONSOLE_SERIAL.printf("[%s] Error: ", request.name());
  
  switch (error) {
    case VitoWiFi::OptolinkResult::TIMEOUT: CONSOLE_SERIAL.println("Timeout"); break;
    case VitoWiFi::OptolinkResult::LENGTH:  CONSOLE_SERIAL.println("Length Mismatch"); break;
    case VitoWiFi::OptolinkResult::NACK:    CONSOLE_SERIAL.println("NACK"); break;
    case VitoWiFi::OptolinkResult::CRC:     CONSOLE_SERIAL.println("CRC Fail"); break;
    case VitoWiFi::OptolinkResult::ERROR:   CONSOLE_SERIAL.println("General Error"); break;
    default:                                CONSOLE_SERIAL.println("Unknown"); break;
  }

  // DAISY CHAIN: Even if error, move to next to prevent getting stuck
  readNextDatapoint();
}

// ----------------------------------------------------------------------------
// HELPER FUNCTIONS
// ----------------------------------------------------------------------------

void readNextDatapoint() {
  // Increment index
  currentDatapointIndex++;

  // Check if we are done with the list
  if (currentDatapointIndex >= NUM_DATAPOINTS) {
    isReadingSequence = false;
    CONSOLE_SERIAL.println("--- Sequence Complete ---\n");
    return;
  }

  // Send read request for the next item
  // Note: VitoWiFi.read is non-blocking, it puts the request in the queue
  if (!vitoWiFi.read(datapoints[currentDatapointIndex])) {
    CONSOLE_SERIAL.printf("Failed to queue request for %s\n", datapoints[currentDatapointIndex].name());
    // If queue fails, try moving to next immediately or abort
    isReadingSequence = false; 
  }
}

void startReadingSequence() {
  if (isReadingSequence) return; // Prevent overlap

  CONSOLE_SERIAL.println("--- Starting Read Sequence ---");
  isReadingSequence = true;
  currentDatapointIndex = 0; // Reset to start
  
  // Trigger the first read manually
  // The rest will be triggered by onResponse/onError
  if (!vitoWiFi.read(datapoints[0])) {
    CONSOLE_SERIAL.println("Failed to start sequence (Queue full?)");
    isReadingSequence = false;
  }
}

// ----------------------------------------------------------------------------
// SETUP & LOOP
// ----------------------------------------------------------------------------

void setup() {
  // Initialize USB Console
  CONSOLE_SERIAL.begin(SERIALBAUDRATE);
  delay(2000); // Wait for USB CDC to enumerate
  CONSOLE_SERIAL.println("Booting ESP32-C3 VitoWiFi...");

  // Connect WiFi
  CONSOLE_SERIAL.printf("Connecting to WiFi SSID: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000UL) {
    delay(250);
    CONSOLE_SERIAL.print('.');
  }
  CONSOLE_SERIAL.println();
  if (WiFi.status() == WL_CONNECTED) {
    CONSOLE_SERIAL.printf("WiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    CONSOLE_SERIAL.println("WiFi not connected (continuing offline)");
  }

  // Setup callbacks
  vitoWiFi.onResponse(onResponse);
  vitoWiFi.onError(onError);

  // Initialize Optolink
  // NOTE: On ESP32-C3 Super Mini, this forces GPIO 20 (RX) and 21 (TX)
  vitoWiFi.begin(); 

  // Minimal web server and ElegantOTA
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "ESP32-C3 VitoWiFi test. OTA at /update");
  });
  ElegantOTA.begin(&server);
  server.begin();
  CONSOLE_SERIAL.println("Web server started; ElegantOTA ready");

  CONSOLE_SERIAL.println("Setup finished. Waiting for timer...");
}

void loop() {
  static uint32_t lastMillis = 0;
  
  // Non-blocking timer: Every 5 seconds
  if (millis() - lastMillis > 5000UL) {
    lastMillis = millis();
    startReadingSequence();
  }

  // Essential: Keep the library state machine running
  vitoWiFi.loop();
  ElegantOTA.loop();
}
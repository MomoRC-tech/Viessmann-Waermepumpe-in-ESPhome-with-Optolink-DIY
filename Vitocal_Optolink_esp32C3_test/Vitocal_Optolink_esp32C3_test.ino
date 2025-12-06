#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#ifndef ELEGANTOTA_USE_ASYNC_WEBSERVER
#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#endif
#include <ElegantOTA.h>

#include <VitoWiFi.h>

#include <WebSerial.h>

IPAddress       local_IP(192, 168, 0, 222);
IPAddress       gateway(192, 168, 0, 1);
IPAddress       subnet(255, 255, 255, 0);
IPAddress       primaryDNS(192, 168, 0, 1);   //optional
IPAddress       secondaryDNS(192, 168, 0, 1);   //optional

// Erstellung des WiFiMulti-Objekts
WiFiMulti WiFiMulti;

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
#define CONSOLE_SERIAL  WebSerial   // configure "Serial" or "WebSerial"
#define SERIALBAUDRATE  115200

// WiFi credentials: prefer local secrets.h, else fallback example
// #if __has_include("secrets.h")
#include "secrets.h"
// #else
// #include "secrets.example.h"
// #endif

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
// Defer scheduling for next read to avoid hammering the queue
static uint32_t nextReadDueMillis = 0;

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
  // Schedule next read with a minimal 2000ms gap
  nextReadDueMillis = millis() + 2000;
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
  // Schedule next attempt after 2000ms to avoid immediate requeue busy
  nextReadDueMillis = millis() + 3000;
}

// ----------------------------------------------------------------------------
// HELPER FUNCTIONS
// ----------------------------------------------------------------------------

void readNextDatapoint() {
  // If we just completed a read, advance to next item
  // Otherwise, we attempt the current index again when busy
  // Check completion (index points to next to read)
  if (currentDatapointIndex >= NUM_DATAPOINTS) {
    isReadingSequence = false;
    CONSOLE_SERIAL.println("--- Sequence Complete ---\n");
    return;
  }

  // Try to queue current datapoint; only increment index on success
  if (vitoWiFi.read(datapoints[currentDatapointIndex])) {
    // Successfully queued; advance index for subsequent step
    currentDatapointIndex++;
  } else {
    // Busy: schedule a retry after >=1000ms, keep sequence active
    CONSOLE_SERIAL.printf("Failed to queue request for %s\n", datapoints[currentDatapointIndex].name());
    nextReadDueMillis = millis() + 3000;
  }
}

void startReadingSequence() {
  if (isReadingSequence) return; // Prevent overlap

  CONSOLE_SERIAL.println("--- Starting Read Sequence ---");
  isReadingSequence = true;
  currentDatapointIndex = 0; // Reset to start
  
  // Trigger the first read via scheduler to ensure 1000ms defer
  nextReadDueMillis = millis() + 1000;
}

// ----------------------------------------------------------------------------
// SETUP & LOOP
// ----------------------------------------------------------------------------

void setup() {
  // Initialize USB Console
  Serial.begin(SERIALBAUDRATE);
  Serial.setDebugOutput(true); // IMPORTANT: ROUTE DEBUG NOT THROUGH THE OPTOLINK SERIAL!
  delay(2000); // Wait for USB CDC to enumerate
  Serial.println("Booting ESP32-C3 VitoWiFi...");

  WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.println();
  Serial.println();
  Serial.print("Waiting for WiFi... ");

  // 2. wait for connection
  WiFi.mode(WIFI_STA);
  // 🔒 Set static IP config BEFORE connecting
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // 💡 workaround for esp32 c3 as of defect antenna design

  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  CONSOLE_SERIAL.println("");
  CONSOLE_SERIAL.println("WiFi connected");
  CONSOLE_SERIAL.print("IP address: ");
  CONSOLE_SERIAL.println(WiFi.localIP());

  // Setup callbacks
  vitoWiFi.onResponse(onResponse);
  vitoWiFi.onError(onError);

  // Initialize Optolink
  // NOTE: On ESP32-C3 Super Mini, this forces GPIO 20 (RX) and 21 (TX)
  vitoWiFi.begin(); 

  // Minimal web server and ElegantOTA
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "ESP32-C3 VitoWiFi test. OTA at /update. Webserial at /webserial");
  });
  ElegantOTA.begin(&server);
  WebSerial.begin(&server);
  server.begin();
  CONSOLE_SERIAL.println("Web server started; ElegantOTA ready");

  CONSOLE_SERIAL.println("Setup finished. Waiting for timer...");
}

void loop() {
  static uint32_t lastMillis = 0;
  
  // Non-blocking timer: Every 5 seconds
  if (millis() - lastMillis > 10000UL) {
    lastMillis = millis();
    startReadingSequence();
  }

  // Essential: Keep the library state machine running
  vitoWiFi.loop();
  ElegantOTA.loop();
  WebSerial.loop();

  // Handle deferred next read when due
  if (isReadingSequence && nextReadDueMillis != 0 && (long)(millis() - nextReadDueMillis) >= 0) {
    // Attempt next queued read step
    readNextDatapoint();
    // Enforce at least 1000ms before next attempt
    nextReadDueMillis = millis() + 2000;
  }
<<<<<<< HEAD
}
=======
}
>>>>>>> ec99e67422b535a16cd78cfe73c8e8eea53071b2

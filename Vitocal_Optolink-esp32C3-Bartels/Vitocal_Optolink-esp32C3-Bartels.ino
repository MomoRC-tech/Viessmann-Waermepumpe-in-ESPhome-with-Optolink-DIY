// ---------------------------------------------------------------------------
// Bartels ESP32-C3/ESP8266 sketch for Viessmann Optolink using VitoWiFi v3
//
// - ESP32-C3: uses Hardware UART0 (GPIO20 RX, GPIO21 TX) for Optolink
// - VitoWiFi handles serial initialization internally on vitowifi.begin()
// - ElegantOTA provides web-based firmware updates (Async server)
// - Polling is grouped and paced via a configurable minimum request gap
// ---------------------------------------------------------------------------
// General includes
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <AsyncTCP.h>
#include "myEveryN.h"

// Runtime measurement (loop duration in µs)
#include <limits.h>  // for UINT32_MAX
static uint32_t rtMinUs     = UINT32_MAX;
static uint32_t rtMaxUs     = 0;
static uint64_t rtSumUs     = 0;
static uint32_t rtSamples   = 0;
static uint32_t rtPrevUs    = 0;

// ElegantOTA configuration: use AsyncWebServer backend
#ifndef ELEGANTOTA_USE_ASYNC_WEBSERVER
  #define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#endif
#if ELEGANTOTA_USE_ASYNC_WEBSERVER
  #pragma message("ElegantOTA async mode enabled")
#else
  #pragma message("ElegantOTA async mode DISABLED")
#endif

// Core libraries and project headers
#include <ElegantOTA.h>
#include <VitoWiFi.h>
#include <ArduinoHA.h>
#include <WebSerial.h>
#include "Vitocal_datapoints.h"
#include "Vitocal_polling.h"

// forward declarations
void onVitoResponse(const uint8_t* data, uint8_t length, const VitoWiFi::Datapoint& request);
void onVitoError(VitoWiFi::OptolinkResult error, const VitoWiFi::Datapoint& request);

// serial config
#define OPTOLINK_SERIAL Serial0
#define CONSOLE_SERIAL  WebSerial   // configure "Serial" or "WebSerial"
#define SERIALBAUDRATE  115200

// Initialize VitoWiFi with the hardware serial port
VitoWiFi::VitoWiFi<VitoWiFi::VS1> vitoWIFI(&OPTOLINK_SERIAL);
 
// Web server configuration and WiFi credentials
#if __has_include("secrets.h")
  #include "secrets.h"  // project-local, git-ignored real credentials
#else
  #include "secrets.example.h" // fallback example values so CI/builds still compile
#endif

const char*     PARAM_INPUT_1 = "output";
const char*     PARAM_INPUT_2 = "state";
IPAddress       local_IP(192, 168, 0, 65);
IPAddress       gateway(192, 168, 0, 1);
IPAddress       subnet(255, 255, 255, 0);
IPAddress       primaryDNS(192, 168, 0, 1);   //optional
IPAddress       secondaryDNS(192, 168, 0, 1);   //optional
AsyncWebServer    server(80);
AsyncEventSource  events("/events");

// MultiWifi object
WiFiMulti WiFiMulti;

// Home Assistant integration
#define BROKER_ADDR             "homeassistant.local"    
#define BROKER_USERNAME         MQTT_USER
#define BROKER_PASSWORD         MQTT_PASS
#define BROKER_PORT             1883

#define DEVICE_NAME             "Waermepumpe_Bartels"
#define DEVICE_SWVERSION        __DATE__ " " __TIME__ 
#define DEVICE_MANUFACTURER     "Viessmann"
#define DEVICE_MODEL            "Vitotronic200-WO1c-Bartels"

#define MQTT_DATAPREFIX         "Technik"
#define MQTT_DISCOVERYPREFIX    "homeassistant"

WiFiClient client;
HADevice device("WPBartels");
HAMqtt mqtt(client, device, 30);


// HA sensors and voids
#include "HA_mqtt_addin.h"

// other
// Loop bookkeeping and simple runtime state
static int     count     = 0;
static int     count_tmp = 0;
static int     eHeiz1    = 0;
static int     eHeiz2    = 0;
static boolean toggle    = false;
// Error handling and health monitoring
volatile uint32_t vitoErrorCount = 0;
volatile uint32_t vitoConsecutiveErrors = 0;
volatile uint32_t vitoErrorThreshold = 30;   // threshold for consecutive errors (configurable via HA)
static const uint32_t vitoErrorWindowMs  = 60000; // window for total errors
uint32_t vitoErrorWindowStartMs = 0;

// Default group intervals tuned for stability vs. throughput
static const uint32_t DEFAULT_FAST_INTERVAL_MS   = 20000UL; // 20s: relays/pumps/compressor/status
static const uint32_t DEFAULT_MEDIUM_INTERVAL_MS = 40000UL; // 40s: temperatures
static const uint32_t DEFAULT_SLOW_INTERVAL_MS   = 60000UL; // 60s: setpoints/hysteresis/heating curve
VitoPollGroupState vitoFastState   = {0, 0, 0, DEFAULT_FAST_INTERVAL_MS};
VitoPollGroupState vitoMediumState = {0, 0, 0, DEFAULT_MEDIUM_INTERVAL_MS};
VitoPollGroupState vitoSlowState   = {0, 0, 0, DEFAULT_SLOW_INTERVAL_MS};

static const char* const operationModeLabels[] = {
  "Abschaltbetrieb",
  "Warmwasser",
  "Heizen und Warmwasser",
  "undefiniert",
  "dauernd reduziert",
  "dauernd normal",
  "normal Abschalt",
  "nur kuehlen"
};

static const char* const manualModeLabels[] = {
  "normal",
  "manuell",
  "WW auf Temp2"
};

static const char* labelOrFallback(uint8_t index, const char* const* table, size_t tableSize)
{
  if (tableSize == 0) {
    return "n/a";
  }
  if (index < tableSize) {
    return table[index];
  }
  return "Unknown";
}

// VitoWiFi datapoint polling groups
// fast: relays, pumps, compressor, error (operational status)
VitoWiFi::Datapoint* vitoFast[] = {
  &dpCompFrequency,
  &dpHeizkreispumpe,
  &dpWWZirkPumpe,
  &dpRelVerdichter,
  &dpRelPrimaerquelle,
  &dpRelSekundaerPumpe,
  &dpRelEHeizStufe1,
  &dpRelEHeizStufe2,
  &dpVentilHeizenWW,
  &dpOperationMode,
  &dpManualMode,
  &dpStoerung
};
const int vitoFastSize = sizeof(vitoFast) / sizeof(vitoFast[0]);

// medium: temperatures
VitoWiFi::Datapoint* vitoMedium[] = {
  &dpTempOutside,
  &dpWWoben,
  &dpVorlaufSoll,
  &dpVorlaufIst,
  &dpRuecklauf
};
const int vitoMediumSize = sizeof(vitoMedium) / sizeof(vitoMedium[0]);

// slow: setpoints, hysteresis, heating curve
VitoWiFi::Datapoint* vitoSlow[] = {
  &dpTempRaumSoll,
  &dpTempRaumSollRed,
  &dpTempWWSoll,
  &dpTempWWSoll2,
  &dpTempHystWWSoll,
  &dpTempHKniveau,
  &dpTempHKNeigung
};
const int vitoSlowSize = sizeof(vitoSlow) / sizeof(vitoSlow[0]);

// Run one paced polling step for a group.
// Each group is polled in rounds. within a round, datapoints are
// queued sequentially with at least minRequestGapMs between requests.
// A new round only starts after the group's intervalMs has elapsed.
void pollVitoGroup(VitoPollGroupState &state,
                   VitoWiFi::Datapoint **group,
                   int groupSize,
                   uint32_t minRequestGapMs)
{
  uint32_t now = millis();

  // do not start a new round before the group interval has passed
  if (state.index == 0 && state.lastRoundEndMs != 0) {
    if ((long)(now - state.lastRoundEndMs) < (long)state.intervalMs) {
      return;
    }
  }

  // enforce minimum gap between individual requests
  if (state.lastRequestMs != 0 && (long)(now - state.lastRequestMs) < (long)minRequestGapMs) {
    return;
  }

  // try to queue next datapoint read, VitoWiFi itself rate-limits via return value
  if (groupSize <= 0) {
    return;
  }

  if (vitoWIFI.read(*group[state.index])) {
    state.lastRequestMs = now;
    state.index++;
    if (state.index >= groupSize) {
      state.index = 0;
      state.lastRoundEndMs = now;
    }
  } else {
    // backoff: don't hammer read() when queue is full
    state.lastRequestMs = now;
  }
}

//## setup#####################################################################
void setup() {  
  // Initialize USB Console
  Serial.begin(SERIALBAUDRATE);
  Serial.setDebugOutput(true); // IMPORTANT: ROUTE DEBUG NOT THROUGH THE OPTOLINK SERIAL!
  delay(2000); // Wait for USB CDC to enumerate
  Serial.println("Booting ESP32-C3 VitoWiFi..."); 

  // add wifi AP  
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
  WiFi.setTxPower(WIFI_POWER_8_5dBm); //  workaround for esp32 c3 as of defect antenna design

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

  // initialise optolink serial and VitoWiFi v3
  vitoWIFI.onResponse(onVitoResponse);
  vitoWIFI.onError(onVitoError);
  vitoWIFI.begin();

  // Minimal web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "ESP32-C3 VitoWiFi test. OTA at /update. Webserial at /webserial");
  });

  // start ota, webserial, server
  ElegantOTA.begin(&server);
  WebSerial.begin(&server);
  server.begin();
  CONSOLE_SERIAL.println("Web server started; ElegantOTA ans WebSerial ready");


  //setup home assistant *******
  setupHomeAssistant();
  
  CONSOLE_SERIAL.println(F("Setup finished..."));
}


//**************************************************
// --- runtime measurement: measure time between loop() iterations ---
void myRuntimeMeasurement()  {
  uint32_t nowUs = micros();
  if (rtPrevUs != 0) {
    uint32_t delta = nowUs - rtPrevUs;  // unsigned handles wrap-around

    if (delta < rtMinUs) rtMinUs = delta;
    if (delta > rtMaxUs) rtMaxUs = delta;
    rtSumUs   += delta;
    rtSamples++;
  }
  rtPrevUs = nowUs;
}

void myPrintRuntime() {
 if (rtSamples > 0) {
      float meanUs = (float)rtSumUs / (float)rtSamples;

      CONSOLE_SERIAL.print(F("[RT] loop Δt (µs): min="));
      CONSOLE_SERIAL.print(rtMinUs);
      CONSOLE_SERIAL.print(F(" max="));
      CONSOLE_SERIAL.print(rtMaxUs);
      CONSOLE_SERIAL.print(F(" mean="));
      CONSOLE_SERIAL.println(meanUs);

      // reset stats window
      rtMinUs   = UINT32_MAX;
      rtMaxUs   = 0;
      rtSumUs   = 0;
      rtSamples = 0;
    } else {
      CONSOLE_SERIAL.println(F("[RT] no samples collected"));
    }
}


//** loop************************************************
void loop() { 
  myRuntimeMeasurement();
  // cyclic VitoWiFi reads (v3 API, grouped for load balancing)
  // each group is polled in rounds -  within a round, datapoints are
  // read sequentially with at least minRequestGapMs between requests.
  // A new round for a group only starts after its intervalMs has passed.

  // Minimum gap between individual VitoWiFi read() calls.
  // Balances Optolink stability (avoid flooding) vs. responsiveness.
  // Configurable defer between individual reads/writes for stability (alpha-compatible)
  #ifndef VITO_MIN_REQUEST_GAP_MS
  #define VITO_MIN_REQUEST_GAP_MS 1500UL
  #endif
  const uint32_t minRequestGapMs = VITO_MIN_REQUEST_GAP_MS;

  pollVitoGroup(vitoFastState,   vitoFast,   vitoFastSize,   minRequestGapMs);
  pollVitoGroup(vitoMediumState, vitoMedium, vitoMediumSize, minRequestGapMs);
  pollVitoGroup(vitoSlowState,   vitoSlow,   vitoSlowSize,   minRequestGapMs);

  // bookkeeping and availability announcement
  EVERY_N_SECONDS (8) {
    count++;
    toggle = !toggle;
    count_tmp++;
    device.publishAvailability();
    CONSOLE_SERIAL.println("VitoWiFi read cycle running");
  }

  // Essential: Keep the library state machine running
  vitoWIFI.loop();
  mqtt.loop();
  ElegantOTA.loop();
  WebSerial.loop();
  
  EVERY_N_SECONDS (300) {
     myCheckWIFIcyclic();
  }

  EVERY_N_SECONDS(4) {
    //myPrintRuntime();
  }

}

//** VitoWiFi response/error handlers (v3) ******************************

void onVitoResponse(const uint8_t* data, uint8_t length, const VitoWiFi::Datapoint& request) {
  VitoWiFi::VariantValue value = request.decode(data, length);

  if (&request == &dpTempOutside) {
    float temp = value;
    AussenTempSens.setValue(temp);
    CONSOLE_SERIAL.println("tmpAu");
  } else if (&request == &dpWWoben) {
    float temp = value;
    WWtempObenSens.setValue(temp);
    CONSOLE_SERIAL.println("WWo");
  } else if (&request == &dpVorlaufSoll) {
    float temp = value;
    VorlaufTempSetSens.setValue(temp);
  } else if (&request == &dpVorlaufIst) {
    float temp = value;
    VorlaufTempSens.setValue(temp);
    HVACwaermepumpe.setCurrentTemperature(temp);
  } else if (&request == &dpRuecklauf) {
    float temp = value;
    RuecklaufTempSens.setValue(temp);
  } else if (&request == &dpCompFrequency) {
    uint8_t v = value;
    CompFrequencySens.setValue(v);
  } else if (&request == &dpRelEHeizStufe1) {
    eHeiz1 = static_cast<uint8_t>(value);
  } else if (&request == &dpRelEHeizStufe2) {
    uint8_t v2 = value;
    eHeiz2 = eHeiz1 + (2 * v2);
    RelEHeizStufeSens.setValue(static_cast<uint8_t>(eHeiz2));
    HVACwaermepumpe.setAuxState(eHeiz2 != 0);
  } else if (&request == &dpHeizkreispumpe) {
    uint8_t v = value;
    heizkreispumpeSens.setState(v);
  } else if (&request == &dpWWZirkPumpe) {
    uint8_t v = value;
    WWzirkulationspumpeSens.setState(v);
  } else if (&request == &dpRelVerdichter) {
    uint8_t v = value;
    RelVerdichterSens.setState(v);
    HVACwaermepumpe.setMode(v ? HAHVAC::HeatMode : HAHVAC::OffMode);
  } else if (&request == &dpRelPrimaerquelle) {
    uint8_t v = value;
    RelPrimaerquelleSens.setState(v);
  } else if (&request == &dpRelSekundaerPumpe) {
    uint8_t v = value;
    RelSekundaerPumpeSens.setState(v);
  } else if (&request == &dpVentilHeizenWW) {
    uint8_t v = value;
    if (v) {
      ventilHeizenWWSens.setValue("Warmwasser");
    } else {
      ventilHeizenWWSens.setValue("Heizen");
    }
  } else if (&request == &dpOperationMode) {
    uint8_t v = value;
    const char* label = labelOrFallback(v, operationModeLabels, sizeof(operationModeLabels) / sizeof(operationModeLabels[0]));
    operationmodeSens.setValue(label);
  } else if (&request == &dpManualMode) {
    uint8_t v = value;
    const char* label = labelOrFallback(v, manualModeLabels, sizeof(manualModeLabels) / sizeof(manualModeLabels[0]));
    manualmodeSens.setValue(label);
    selectManualMode.setState(v);
  } else if (&request == &dpTempRaumSoll) {
    float t = value;
    RaumSollTempSens.setState(t);
    HVACwaermepumpe.setTargetTemperature(t);
  } else if (&request == &dpTempRaumSollRed) {
    float t = value;
    RaumSollRedSens.setState(t);
  } else if (&request == &dpTempWWSoll) {
    float t = value;
    WWtempSollSens.setState(t);
  } else if (&request == &dpTempWWSoll2) {
    float t = value;
    WWtempSoll2Sens.setState(t);
  } else if (&request == &dpTempHystWWSoll) {
    float t = value;
    HystWWsollSens.setState(t);
  } else if (&request == &dpTempHKniveau) {
    float t = value;
    HKniveauSens.setState(t);
  } else if (&request == &dpTempHKNeigung) {
    float t = value;
    HKneigungSens.setState(t);
  }
}

void onVitoError(VitoWiFi::OptolinkResult error, const VitoWiFi::Datapoint& request) {
  // Record error diagnostics and apply simple recovery/backoff if needed.
  CONSOLE_SERIAL.print("VitoWiFi error for ");
  CONSOLE_SERIAL.print(request.name());
  CONSOLE_SERIAL.print(": ");
  CONSOLE_SERIAL.println(static_cast<int>(error));

  // Track errors: consecutive and within a window
  uint32_t now = millis();
  vitoConsecutiveErrors++;
  if (vitoErrorWindowStartMs == 0 || (now - vitoErrorWindowStartMs) > vitoErrorWindowMs) {
    vitoErrorWindowStartMs = now;
    vitoErrorCount = 0;
  }
  vitoErrorCount++;

  // Publish diagnostic counters to HA
  vitoErrorCountSens.setValue(vitoErrorCount);
  vitoConsecErrorSens.setValue(vitoConsecutiveErrors);

  // Simple recovery -  if too many consecutive errors, briefly pause polling and try to kick VitoWiFi
  if (vitoConsecutiveErrors >= vitoErrorThreshold) {
    CONSOLE_SERIAL.println("Too many consecutive VitoWiFi errors; applying backoff and reinitializing VitoWiFi...");
    // Backoff by delaying further reads for a short period via intervals increase
    vitoFastState.intervalMs   = 30000UL;
    vitoMediumState.intervalMs = 60000UL;
    vitoSlowState.intervalMs   = 90000UL;
    // Attempt a light reinit
    vitoWIFI.end();
    delay(200);
    vitoWIFI.begin();
    vitoConsecutiveErrors = 0;
  }
}


//************************************************************


void myCheckWIFIcyclic () {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    // WIFI is connected  
  }
  else {
    Serial.println("not reconnected!");
  }
}
// ---------------------------------------------------------------------------
// BARTELS: ESP32-C3/ESP8266 sketch for Viessmann Optolink using VitoWiFi v3
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
#include <string.h>  // for strcmp

// forward declarations
void onVitoResponse(const uint8_t* data, uint8_t length, const VitoWiFi::Datapoint& request);
void onVitoError(VitoWiFi::OptolinkResult error, const VitoWiFi::Datapoint& request);

// serial config
#define OPTOLINK_SERIAL Serial0
#define CONSOLE_SERIAL  WebSerial   // configure "Serial" or "WebSerial"
#define SERIALBAUDRATE  115200

// Initialize VitoWiFi with the hardware serial port
VitoWiFi::VitoWiFi<VitoWiFi::VS1> vitoWIFI(&OPTOLINK_SERIAL);

inline bool isDp(const VitoWiFi::Datapoint& req, const VitoWiFi::Datapoint& dp) {
    return strcmp(req.name(), dp.name()) == 0;
}

// Web server configuration and WiFi credentials
#if __has_include("secrets.h")
  #include "secrets.h"  // project-local, git-ignored real credentials
#else
  #include "secrets.example.h" // fallback example values so CI/builds still compile
#endif

const char*     PARAM_INPUT_1 = "output";
const char*     PARAM_INPUT_2 = "state";
IPAddress       local_IP(192, 168, 0, 60);
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
static const uint32_t DEFAULT_FAST_INTERVAL_MS   = 60000UL; // relays/pumps/compressor/status
static const uint32_t DEFAULT_MEDIUM_INTERVAL_MS = 85000UL; // temperatures
static const uint32_t DEFAULT_SLOW_INTERVAL_MS   = 180000UL; // setpoints/hysteresis/heating curve
VitoPollGroupState vitoFastState   = {0, 0, 0, DEFAULT_FAST_INTERVAL_MS};
VitoPollGroupState vitoMediumState = {0, 0, 0, DEFAULT_MEDIUM_INTERVAL_MS};
VitoPollGroupState vitoSlowState   = {0, 0, 0, DEFAULT_SLOW_INTERVAL_MS};

// Global VitoWiFi scheduling state:
// - at most one in-flight request at a time
// - enforce a small gap after each response/error
#ifndef VITO_RESPONSE_GAP_MS
#define VITO_RESPONSE_GAP_MS 100UL   // ms after each response before next request
#endif
static const uint32_t vitoResponseGapMs = VITO_RESPONSE_GAP_MS;

static bool     vitoBusy           = false; // true while we wait for a response
static uint32_t vitoLastResponseMs = 0;     // millis() when last response/error arrived

// labels
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

// per-DP last-success timestamps (ms since boot)
static uint32_t lastTempOutsideMs      = 0;
static uint32_t lastWWobenMs           = 0;
static uint32_t lastVorlaufSollMs      = 0;
static uint32_t lastVorlaufIstMs       = 0;
static uint32_t lastRuecklaufMs        = 0;
static uint32_t lastRelEHeiz1Ms        = 0;
static uint32_t lastRelEHeiz2Ms        = 0;
static uint32_t lastHeizkreispumpeMs   = 0;
static uint32_t lastWWZirkPumpeMs      = 0;
static uint32_t lastRelVerdichterMs    = 0;
static uint32_t lastRelPrimaerMs       = 0;
static uint32_t lastRelSekundaerMs     = 0;
static uint32_t lastVentilHeizenWWMs   = 0;
static uint32_t lastOperationModeMs    = 0;
static uint32_t lastManualModeMs       = 0;
static uint32_t lastRaumSollMs         = 0;
static uint32_t lastRaumSollRedMs      = 0;
static uint32_t lastWWSollMs           = 0;
static uint32_t lastWWSoll2Ms          = 0;
static uint32_t lastHystWWSollMs       = 0;
static uint32_t lastHKniveauMs         = 0;
static uint32_t lastHKneigungMs        = 0;

// --- Per-datapoint timing info: request -> response round-trip ----------

struct DpTimingInfo {
    const VitoWiFi::Datapoint* dp;
    uint32_t lastRequestMs;   // when we queued the read()
};

DpTimingInfo dpTiming[] = {
    { &dpTempOutside,      0 },
    { &dpWWoben,           0 },
    { &dpVorlaufSoll,      0 },
    { &dpVorlaufIst,       0 },
    { &dpRuecklauf,        0 },
    { &dpRelEHeizStufe1,   0 },
    { &dpRelEHeizStufe2,   0 },
    { &dpHeizkreispumpe,   0 },
    { &dpWWZirkPumpe,      0 },
    { &dpRelVerdichter,    0 },
    { &dpRelPrimaerquelle, 0 },
    { &dpRelSekundaerPumpe,0 },
    { &dpVentilHeizenWW,   0 },
    { &dpOperationMode,    0 },
    { &dpManualMode,       0 },
    { &dpTempRaumSoll,     0 },
    { &dpTempRaumSollRed,  0 },
    { &dpTempWWSoll,       0 },
    { &dpTempWWSoll2,      0 },
    { &dpTempHystWWSoll,   0 },
    { &dpTempHKniveau,     0 },
    { &dpTempHKNeigung,    0 },
    { &dpStoerung,         0 }
};

constexpr size_t dpTimingCount = sizeof(dpTiming) / sizeof(dpTiming[0]);


// VitoWiFi datapoint polling groups
// fast: relays, pumps, compressor, error (operational status)
VitoWiFi::Datapoint* vitoFast[] = {
  &dpHeizkreispumpe,
  &dpWWZirkPumpe,
  &dpRelVerdichter,
  &dpRelPrimaerquelle,
  &dpRelSekundaerPumpe,
  &dpRelEHeizStufe1,
  &dpRelEHeizStufe2,
  &dpVentilHeizenWW,
  &dpStoerung
};
const int vitoFastSize = sizeof(vitoFast) / sizeof(vitoFast[0]);

// medium: temperatures
VitoWiFi::Datapoint* vitoMedium[] = {
  &dpTempOutside,
  &dpWWoben,
  &dpVorlaufSoll,
  &dpVorlaufIst,
  &dpRuecklauf,
  &dpOperationMode,
  &dpManualMode,
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

// --- per-DP timing helpers -------------------------------------
inline void logDpFloat(const char* tag, float val, uint32_t& lastMs) {
    uint32_t now = millis();
    uint32_t dt  = lastMs ? (now - lastMs) : 0;
    lastMs = now;

    CONSOLE_SERIAL.print(tag);
    CONSOLE_SERIAL.print(": ");
    CONSOLE_SERIAL.print(val, 1);
    if (dt) {
        CONSOLE_SERIAL.print(" (Δt=");
        CONSOLE_SERIAL.print(dt);
        CONSOLE_SERIAL.print(" ms)");
    }
    CONSOLE_SERIAL.println();
}

inline void logDpUint(const char* tag, uint8_t v, uint32_t& lastMs) {
    uint32_t now = millis();
    uint32_t dt  = lastMs ? (now - lastMs) : 0;
    lastMs = now;

    CONSOLE_SERIAL.print(tag);
    CONSOLE_SERIAL.print(": ");
    CONSOLE_SERIAL.print(v);
    if (dt) {
        CONSOLE_SERIAL.print(" (Δt=");
        CONSOLE_SERIAL.print(dt);
        CONSOLE_SERIAL.print(" ms)");
    }
    CONSOLE_SERIAL.println();
}

inline void logDpMode(const char* tag, uint8_t v, const char* label, uint32_t& lastMs) {
    uint32_t now = millis();
    uint32_t dt  = lastMs ? (now - lastMs) : 0;
    lastMs = now;

    CONSOLE_SERIAL.print(tag);
    CONSOLE_SERIAL.print(": ");
    CONSOLE_SERIAL.print(v);
    CONSOLE_SERIAL.print(" -> ");
    CONSOLE_SERIAL.print(label);
    if (dt) {
        CONSOLE_SERIAL.print(" (Δt=");
        CONSOLE_SERIAL.print(dt);
        CONSOLE_SERIAL.print(" ms)");
    }
    CONSOLE_SERIAL.println();
}


// Run one paced polling step for a group.
// - intervalMs: minimum time between start-of-round to start-of-next-round
// - responseGapMs: minimum time after last response/error before any new request
// Returns true if a request was actually queued.
bool pollVitoGroup(
    VitoPollGroupState &state,
    VitoWiFi::Datapoint **group,
    int groupSize,
    uint32_t responseGapMs
) {
    uint32_t now = millis();

    if (groupSize <= 0) {
        return false;
    }

    // 0) Only one request in flight at any time.
    if (vitoBusy) {
        return false;
    }

    // 1) Enforce global gap after last response/error
    if (vitoLastResponseMs != 0 &&
        (long)(now - vitoLastResponseMs) < (long)responseGapMs) {
        return false;
    }

    // 2) Respect group start-to-start interval
    if (state.index == 0 && state.lastRoundEndMs != 0) {
        if ((long)(now - state.lastRoundEndMs) < (long)state.intervalMs) {
            return false;
        }
    }

    // 3) Safety: keep index in range
    if (state.index >= (uint8_t)groupSize) {
        state.index = 0;
    }

    VitoWiFi::Datapoint* dp = group[state.index];

    // 4) Try to queue the next datapoint.
    if (vitoWIFI.read(*dp)) {
        // We successfully queued one request.
        vitoBusy = true;
        state.lastRequestMs = now;

        // remember when this particular DP was requested
        for (size_t i = 0; i < dpTimingCount; ++i) {
            if (isDp(*dpTiming[i].dp, *dp)) {
                dpTiming[i].lastRequestMs = now;
                break;
            }
        }

        if (state.index == 0) {
            state.lastRoundEndMs = now;  // start-of-round timestamp
        }

        state.index++;
        if (state.index >= groupSize) {
            state.index = 0;
        }

        return true;
    }

    // VitoWiFi refused the request (busy) -> keep state.index as-is
    // so we can retry the same datapoint later.
    return false;
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
  // We schedule at most ONE new request per loop iteration
  bool queued = false;

  // Priority: fast -> medium -> slow
  if (!queued) queued = pollVitoGroup(vitoFastState,   vitoFast,   vitoFastSize,   vitoResponseGapMs);
  if (!queued) queued = pollVitoGroup(vitoMediumState, vitoMedium, vitoMediumSize, vitoResponseGapMs);
  if (!queued) queued = pollVitoGroup(vitoSlowState,   vitoSlow,   vitoSlowSize,   vitoResponseGapMs);

  // (If you still want the test group during debugging, put it here and
  // guard with #if / #else so you don't poll dpTempOutside twice.)

  EVERY_N_SECONDS(8) {
    count++;
    toggle = !toggle;
    device.publishAvailability();
    CONSOLE_SERIAL.println("VitoWiFi read cycle running");
  }

  // Essential: Keep the library state machine running
  vitoWIFI.loop();
  mqtt.loop();
  ElegantOTA.loop();
  WebSerial.loop();

  EVERY_N_SECONDS(300) {
    myCheckWIFIcyclic();
  }

  EVERY_N_SECONDS(4) {
    // myPrintRuntime();
  }
}


//** VitoWiFi response/error handlers (v3) ******************************
void onVitoResponse(const uint8_t* data, uint8_t length, const VitoWiFi::Datapoint& request) {
    vitoBusy = false;
    uint32_t nowMs = millis();
    vitoLastResponseMs = nowMs;

    // compute time between request and this response
    uint32_t dtReqMs = 0;
    for (size_t i = 0; i < dpTimingCount; ++i) {
        if (isDp(request, *dpTiming[i].dp)) {
            if (dpTiming[i].lastRequestMs != 0) {
                dtReqMs = nowMs - dpTiming[i].lastRequestMs;
            }
            break;
        }
    }

    VitoWiFi::VariantValue value = request.decode(data, length);
    const char* name = request.name();

    CONSOLE_SERIAL.print("onVitoResponse for ");
    CONSOLE_SERIAL.print(name);
    CONSOLE_SERIAL.print(" (Δreq=");
    CONSOLE_SERIAL.print(dtReqMs);
    CONSOLE_SERIAL.println(" ms)");

    if (isDp(request, dpTempOutside)) {
        float temp = value;
        AussenTempSens.setValue(temp);
        logDpFloat("tmpAu (AussenTemp)", temp, lastTempOutsideMs);

    } else if (isDp(request, dpWWoben)) {
        float temp = value;
        WWtempObenSens.setValue(temp);
        logDpFloat("WWo (WWtempOben)", temp, lastWWobenMs);

    } else if (isDp(request, dpVorlaufSoll)) {
        float temp = value;
        VorlaufTempSetSens.setValue(temp);
        logDpFloat("VorlaufSoll", temp, lastVorlaufSollMs);

    } else if (isDp(request, dpVorlaufIst)) {
        float temp = value;
        VorlaufTempSens.setValue(temp);
        HVACwaermepumpe.setCurrentTemperature(temp);
        logDpFloat("VorlaufIst", temp, lastVorlaufIstMs);

    } else if (isDp(request, dpRuecklauf)) {
        float temp = value;
        RuecklaufTempSens.setValue(temp);
        logDpFloat("Ruecklauf", temp, lastRuecklaufMs);

    } else if (isDp(request, dpRelEHeizStufe1)) {
        eHeiz1 = static_cast<uint8_t>(value);
        logDpUint("RelEHeizStufe1 (raw)", eHeiz1, lastRelEHeiz1Ms);

    } else if (isDp(request, dpRelEHeizStufe2)) {
        uint8_t v2 = value;
        eHeiz2 = eHeiz1 + (2 * v2);
        RelEHeizStufeSens.setValue(static_cast<uint8_t>(eHeiz2));
        HVACwaermepumpe.setAuxState(eHeiz2 != 0);
        logDpUint("RelEHeizStufe2 (combined)", eHeiz2, lastRelEHeiz2Ms);

    } else if (isDp(request, dpHeizkreispumpe)) {
        uint8_t v = value;
        heizkreispumpeSens.setState(v);
        logDpUint("Heizkreispumpe", v, lastHeizkreispumpeMs);

    } else if (isDp(request, dpWWZirkPumpe)) {
        uint8_t v = value;
        WWzirkulationspumpeSens.setState(v);
        logDpUint("WWZirkulationspumpe", v, lastWWZirkPumpeMs);

    } else if (isDp(request, dpRelVerdichter)) {
        uint8_t v = value;
        RelVerdichterSens.setState(v);
        HVACwaermepumpe.setMode(v ? HAHVAC::HeatMode : HAHVAC::OffMode);
        logDpUint("RelVerdichter", v, lastRelVerdichterMs);

    } else if (isDp(request, dpRelPrimaerquelle)) {
        uint8_t v = value;
        RelPrimaerquelleSens.setState(v);
        logDpUint("RelPrimaerquelle", v, lastRelPrimaerMs);

    } else if (isDp(request, dpRelSekundaerPumpe)) {
        uint8_t v = value;
        RelSekundaerPumpeSens.setState(v);
        logDpUint("RelSekundaerPumpe", v, lastRelSekundaerMs);

    } else if (isDp(request, dpVentilHeizenWW)) {
        uint8_t v = value;
        const char* text = v ? "Warmwasser" : "Heizen";
        ventilHeizenWWSens.setValue(text);
        logDpMode("ventilHeizenWW", v, text, lastVentilHeizenWWMs);

    } else if (isDp(request, dpOperationMode)) {
        uint8_t v = value;
        const char* label = labelOrFallback(
            v, operationModeLabels, sizeof(operationModeLabels) / sizeof(operationModeLabels[0])
        );
        operationmodeSens.setValue(label);
        logDpMode("operationmode", v, label, lastOperationModeMs);

    } else if (isDp(request, dpManualMode)) {
        uint8_t v = value;
        const char* label = labelOrFallback(
            v, manualModeLabels, sizeof(manualModeLabels) / sizeof(manualModeLabels[0])
        );
        manualmodeSens.setValue(label);
        selectManualMode.setState(v);
        logDpMode("manualmode", v, label, lastManualModeMs);

    } else if (isDp(request, dpTempRaumSoll)) {
        float t = value;
        RaumSollTempSens.setState(t);
        HVACwaermepumpe.setTargetTemperature(t);
        logDpFloat("RaumSollTemp", t, lastRaumSollMs);

    } else if (isDp(request, dpTempRaumSollRed)) {
        float t = value;
        RaumSollRedSens.setState(t);
        logDpFloat("RaumSollRed", t, lastRaumSollRedMs);

    } else if (isDp(request, dpTempWWSoll)) {
        float t = value;
        WWtempSollSens.setState(t);
        logDpFloat("WWtempSoll", t, lastWWSollMs);

    } else if (isDp(request, dpTempWWSoll2)) {
        float t = value;
        WWtempSoll2Sens.setState(t);
        logDpFloat("WWtempSoll2", t, lastWWSoll2Ms);

    } else if (isDp(request, dpTempHystWWSoll)) {
        float t = value;
        HystWWsollSens.setState(t);
        logDpFloat("TempHystWWSoll", t, lastHystWWSollMs);

    } else if (isDp(request, dpTempHKniveau)) {
        float t = value;
        HKniveauSens.setState(t);
        logDpFloat("TempHKniveau", t, lastHKniveauMs);

    } else if (isDp(request, dpTempHKNeigung)) {
        float t = value;
        HKneigungSens.setState(t);
        logDpFloat("TempHKNeigung", t, lastHKneigungMs);
    }
}


void onVitoError(VitoWiFi::OptolinkResult error, const VitoWiFi::Datapoint& request) {
  vitoBusy = false;
  vitoLastResponseMs = millis();

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
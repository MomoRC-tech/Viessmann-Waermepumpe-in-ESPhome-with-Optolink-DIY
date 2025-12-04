//###############################
/*
This example defines three datapoints.
The first two are DPTemp type datapoints and have their own callback.
When no specific callback is attached to a datapoint, it uses the global callback.

Note the difference in return value between the callbacks:
for tempCallback uses value.getFloat() as DPTemp datapoints return a float.
globalCallback uses value.getString(char*,size_t). This method is independent of the returned type.
*/
//###############################
#ifdef ESP32
  #include <WiFi.h>
  #include <ESP32Ping.h>
  #include <AsyncTCP.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  // only if you really use ping – right now the ping function is commented out:
  // #include <ESP8266Ping.h> 
  #include <ESPAsyncTCP.h>
  #include <SoftwareSerial.h>
#endif

// #include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#ifndef ELEGANTOTA_USE_ASYNC_WEBSERVER
#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#endif
#if ELEGANTOTA_USE_ASYNC_WEBSERVER
#pragma message("ElegantOTA async mode enabled")
#else
#pragma message("ElegantOTA async mode DISABLED")
#endif
#include <ElegantOTA.h>
// VitoWiFi v3
#include <VitoWiFi.h>
#include "Vitocal_datapoints.h"
#include "Vitocal_polling.h"
// #include "Vitocal_common.h"
#include <FastLED.h>
#include <WebSerial.h>

#if defined(ESP8266)
static constexpr uint8_t OPTOLINK_RX_PIN = D1;  // GPIO5
static constexpr uint8_t OPTOLINK_TX_PIN = D2;  // GPIO4
SoftwareSerial optolinkSerial(OPTOLINK_RX_PIN, OPTOLINK_TX_PIN, false);
VitoWiFi::VitoWiFi<VitoWiFi::VS1> vito(&optolinkSerial);
#elif defined(ESP32)
#if CONFIG_IDF_TARGET_ESP32C3
HardwareSerial& optolinkSerial = Serial0; // ESP32-C3: use UART0 (GPIO20 RX, GPIO21 TX)
#else
HardwareSerial& optolinkSerial = Serial1; // Other ESP32 targets: keep UART1 default
#endif
VitoWiFi::VitoWiFi<VitoWiFi::VS1> vito(&optolinkSerial);
#else
VitoWiFi::VitoWiFi<VitoWiFi::VS1> vito(&Serial);
#endif

// reset reasons
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
// #include "rom/rtc.h"
#endif
 
//** webserver************************************************
#if __has_include("secrets.h")
#include "secrets.h"  // project-local, git-ignored real credentials
#else
#include "secrets.example.h" // fallback example values so CI/builds still compile
#endif
const char*     PARAM_INPUT_1 = "output";
const char*     PARAM_INPUT_2 = "state";
IPAddress       local_IP(192, 168, 0, 65);
IPAddress       gateway(192, 168, 0, 1);
IPAddress       subnet(255, 255, 0, 0);
IPAddress       primaryDNS(192, 168, 0, 1);   //optional
IPAddress       secondaryDNS(192, 168, 0, 1);   //optional
AsyncWebServer    server(80);
AsyncEventSource  events("/events");

// home assistant-------------------------------------------------------------
#include <ArduinoHA.h>

#define BROKER_ADDR             "homeassistant.local"    
#define BROKER_USERNAME         MQTT_USER
#define BROKER_PASSWORD         MQTT_PASS
#define BROKER_PORT             1883

#define DEVICE_NAME             "Waermepumpe_Bartels"
#if __has_include("version_autogen.h")
#include "version_autogen.h"
#else
#define DEVICE_SWVERSION        "0.2.5"
#endif
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
  "manuel",
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

// helper to run one polling step for a group
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

  // try to queue next datapoint read; VitoWiFi itself rate-limits via return value
  if (groupSize <= 0) {
    return;
  }

  if (vito.read(*group[state.index])) {
    state.lastRequestMs = now;
    state.index++;
    if (state.index >= groupSize) {
      state.index = 0;
      state.lastRoundEndMs = now;
    }
  }
}

// helper to run one polling step for a group
//## setup#####################################################################
void setup() {
  setupVitoWifi();  
  mySetupWIFI();
  myConnect2WIFI();
  myStartAsyncServer();
  //setup home assistant *******
  setupHomeAssistant();
  
  WebSerial.println(F("Setup finished..."));
}

//** loop************************************************
void loop() { 
  // cyclic VitoWiFi reads (v3 API, grouped for load balancing)
  // each group is polled in rounds; within a round, datapoints are
  // read sequentially with at least minRequestGapMs between requests.
  // A new round for a group only starts after its intervalMs has passed.

  // Minimum gap between individual VitoWiFi read() calls.
  // Balances Optolink stability (avoid flooding) vs. responsiveness.
  // Configurable defer between individual reads/writes for stability (alpha-compatible)
  #ifndef VITO_MIN_REQUEST_GAP_MS
  #define VITO_MIN_REQUEST_GAP_MS 1000UL
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
    WebSerial.println("VitoWiFi read cycle running");
  }

  vito.loop();
  mqtt.loop();
  ElegantOTA.loop();
  
  EVERY_N_SECONDS (61) {
     myCheckWIFIcyclic();
  }
  EVERY_N_SECONDS (300) {
	if (WiFi.status() != WL_CONNECTED) {
	  ESP.restart();
	}
  }
}

// forward declaration of the handlers (VitoWiFi v3)
void onVitoResponse(const uint8_t* data, uint8_t length, const VitoWiFi::Datapoint& request);
void onVitoError(VitoWiFi::OptolinkResult error, const VitoWiFi::Datapoint& request);

//************************************************************
void setupVitoWifi () {
  // initialise optolink serial and VitoWiFi v3
  #if defined(ESP8266)
    optolinkSerial.begin(4800, SWSERIAL_8E1, OPTOLINK_RX_PIN, OPTOLINK_TX_PIN, false);
    optolinkSerial.enableRxGPIOPullUp(true);
  #elif defined(ESP32)
    #if CONFIG_IDF_TARGET_ESP32C3
      // ESP32-C3: leave pins at defaults for UART0 (GPIO20 RX, GPIO21 TX)
      optolinkSerial.begin(4800, SERIAL_8E1);
    #else
      optolinkSerial.begin(4800, SERIAL_8E1, 16, 17);
    #endif
  #else
    Serial.begin(4800, SERIAL_8E1);
  #endif

  vito.onResponse(onVitoResponse);
  vito.onError(onVitoError);
  vito.begin();
}
//** VitoWiFi response/error handlers (v3) ******************************

void onVitoResponse(const uint8_t* data, uint8_t length, const VitoWiFi::Datapoint& request) {
  VitoWiFi::VariantValue value = request.decode(data, length);

  if (&request == &dpTempOutside) {
    float temp = value;
    AussenTempSens.setValue(temp);
    WebSerial.println("tmpAu");
  } else if (&request == &dpWWoben) {
    float temp = value;
    WWtempObenSens.setValue(temp);
    WebSerial.println("WWo");
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
    RelEHeizStufeSens.setValue(eHeiz2);
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
  WebSerial.print("VitoWiFi error for ");
  WebSerial.print(request.name());
  WebSerial.print(": ");
  WebSerial.println(static_cast<int>(error));

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

  // Simple recovery: if too many consecutive errors, briefly pause polling and try to kick VitoWiFi
  if (vitoConsecutiveErrors >= vitoErrorThreshold) {
    WebSerial.println("Too many consecutive VitoWiFi errors; applying backoff and reinitializing VitoWiFi...");
    // Backoff by delaying further reads for a short period via intervals increase
    vitoFastState.intervalMs   = 30000UL;
    vitoMediumState.intervalMs = 60000UL;
    vitoSlowState.intervalMs   = 90000UL;
    // Attempt a light reinit
    vito.end();
    delay(200);
    vito.begin();
    vitoConsecutiveErrors = 0;
  }
}

//** other voids ************************************************
void myStartAsyncServer() {
  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am Momo VitocalWIFI_MQTT.");
  });
  // Send a GET request to <ESP_IP>/updatevalues?output=<inputMessage1>&state=<inputMessage2>
  server.on("/updateValues", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage1;
    String inputMessage2;
    // GET input1 value on <ESP_IP>/updatevalues?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
    }
    else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }
    request->send(200, "text/plain", "OK");
    
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      //if (dbgSer) Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  ElegantOTA.begin(&server);
  WebSerial.begin(&server);
  server.begin();
  WebSerial.println("HTTP server started");
}

void mySetupWIFI() {
  WiFi.mode(WIFI_OFF);
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    WebSerial.println("STA Failed to configure");
  }

#ifdef ESP32
  WiFi.setHostname("VitocalWIFIbartels");
#elif defined(ESP8266)
  WiFi.hostname("VitocalWIFIbartels");
#endif

  WiFi.mode(WIFI_STA);
}

void myConnect2WIFI () {
  int _time;
  // initially connect to WIFI with timeout
  WebSerial.print("Connecting to ");
  WebSerial.println(WIFI_SSID);
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  _time = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if ((millis()-_time) > 10000) {         // restart if not connected after 10s
      ESP.restart();
      } else {
      WebSerial.print(".");
    }
  }
  WebSerial.println("");
  WebSerial.println("WiFi connected.");
}

void myCheckWIFIcyclic () {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    // WIFI is connected  
  }
  else {
    WebSerial.println("not reconnected!");
  }
}



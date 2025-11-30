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
#endif

// #include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <VitoWiFi.h>
// #include "Vitocal_common.h"
#include "Vitocal_config.h"
#include <FastLED.h>
#include <WebSerial.h>

// vitowifi
VitoWiFi::VitoWiFi<VitoWiFi::VS1> vito(&Serial); 

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
const char*     ssid =          "MOMOWLAN";
const char*     password =      "GandalfF2";
const char*     PARAM_INPUT_1 = "output";
const char*     PARAM_INPUT_2 = "state";
IPAddress       local_IP(192, 168, 0, 65);
IPAddress       gateway(192, 168, 0, 1);
IPAddress       subnet(255, 255, 0, 0);
IPAddress       primaryDNS(192, 168, 0, 1);   //optional
IPAddress       secondaryDNS(192, 168, 0, 1);   //optional
const char*     googleAddress = "www.google.com";
int             pingResult;
AsyncWebServer    server(80);
AsyncEventSource  events("/events");

//** debug
boolean dbgSer = true;


// home assistant-------------------------------------------------------------
#include <ArduinoHA.h>

#define BROKER_ADDR             "homeassistant.local"    
#define BROKER_USERNAME         "mqtt-user" 
#define BROKER_PASSWORD         "momo1234"
#define BROKER_PORT             1883

#define DEVICE_NAME             "Waermepumpe_Bartels"
#define DEVICE_SWVERSION        "1.0.0"
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
int     count     = 0;
int     count_tmp = 0;
int     eHeiz1 = 0;
int     eHeiz2 = 0;
boolean toggle = false;


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
     
  EVERY_N_SECONDS (8) {
        if (count == 0) {
            VitoWiFi.readGroup("status3");
        } else if (count == 1){
            VitoWiFi.readGroup("temp2");
        } else if (count == 2){
            VitoWiFi.readGroup("status1");
        } else if (count == 3){
            VitoWiFi.readGroup("status2");
        } else if (count == 4){
            VitoWiFi.readGroup("temp");
        } else if (count == 5){
            VitoWiFi.readGroup("temp3");
        } 
        count++;
        toggle = !toggle;
        if (count == 6) {count = 0;}
        count_tmp++;
        device.publishAvailability();
        WebSerial.println("readGroup sent");
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

// forward declaration of the handler 
void onVitoResponse(const VitoWiFi::PacketVS1& response, const VitoWiFi::Datapoint& request);

void onVitoError(VitoWiFi::OptolinkResult error, const VitoWiFi::Datapoint& request);

//************************************************************
void setupVitoWifi () {
  // ... setCallback() for each DP ...
  VitoWiFi.setGlobalCallback(globalCallbackHandler);
  VitoWiFi.disableLogger();
  #ifdef ESP32
    VitoWiFi.setup(&Serial1, 16, 17);
  #elif defined(ESP8266)
    VitoWiFi.setup(&Serial);
  #endif
}

//** voids VitoWIFI************************************************

void tempOutsideCallbackHandler(const IDatapoint& dp, DPValue value) {
  AussenTempSens.setValue(value.getFloat());
  WebSerial.println("tmpAu");
}
void tempWWobenCallbackHandler(const IDatapoint& dp, DPValue value) {
  WWtempObenSens.setValue(value.getFloat());
  WebSerial.println("WWo");
}

void tempVorlaufSollCallbackHandler(const IDatapoint& dp, DPValue value) {
  VorlaufTempSetSens.setValue(value.getFloat());
  // WebSerial.println("VLsoll");
}
void tempVorlaufCallbackHandler(const IDatapoint& dp, DPValue value) {
    VorlaufTempSens.setValue(value.getFloat());
    HVACwaermepumpe.setCurrentTemperature(value.getFloat());
    // WebSerial.println("VL");
}
void tempRuecklaufCallbackHandler(const IDatapoint& dp, DPValue value) {
  RuecklaufTempSens.setValue(value.getFloat());
  // WebSerial.println("RL");
}
void compFreqCallbackHandler(const IDatapoint& dp, DPValue value) {
  CompFrequencySens.setValue(value.getU8());
  // WebSerial.println("CompFreq");
}

void EHeizStufe1CallbackHandler(const IDatapoint& dp, DPValue value) {
   eHeiz1 = value.getU8();
}
void EHeizStufe2CallbackHandler(const IDatapoint& dp, DPValue value) {
      eHeiz2 = eHeiz1 + (2 * value.getU8());
      RelEHeizStufeSens.setValue(eHeiz2); 
      // WebSerial.println("Eheiz2");
       
      if (eHeiz2) {
        HVACwaermepumpe.setAuxState(true);        
      } else {
        HVACwaermepumpe.setAuxState(false);        
      }
}


//------------------------------------------------------------------

void heizkreisPumpeCallbackHandler(const IDatapoint& dp, DPValue value) {
      heizkreispumpeSens.setState(value.getU8()); 
}
void WWzirkPumpeCallbackHandler(const IDatapoint& dp, DPValue value) {
      WWzirkulationspumpeSens.setState(value.getU8()); 
}
void RelVerdichterCallbackHandler(const IDatapoint& dp, DPValue value) {
      uint8_t _val = value.getU8();
      RelVerdichterSens.setState(_val); 
      // WebSerial.print("Verdichter - ");
      // WebSerial.println(_val);
      if (_val) {   
        HVACwaermepumpe.setMode(HAHVAC::HeatMode);
      } else {
        HVACwaermepumpe.setMode(HAHVAC::OffMode);
      }    
}
void RelPrimaerquelleCallbackHandler(const IDatapoint& dp, DPValue value) {
      RelPrimaerquelleSens.setState(value.getU8()); 
      // WebSerial.print("Brunnenpump - ");
      // WebSerial.println(value.getU8());
}
void RelSekundaerPumpeCallbackHandler(const IDatapoint& dp, DPValue value) {
      RelSekundaerPumpeSens.setState(value.getU8());
      // WebSerial.print("Sek.pump - ");
      // WebSerial.println(value.getU8()); 
}
void ventilHeizenWWCallbackHandler(const IDatapoint& dp, DPValue value) {
      bool _val = value.getU8();
      if (_val) {
         ventilHeizenWWSens.setValue("WW"); 
      } else {
         ventilHeizenWWSens.setValue("Heizen");
      }
}
void operationmodeCallbackHandler(const IDatapoint& dp, DPValue value) {
     int _val = value.getU8();
     String _str =  opmodes[_val];
     char _buf[21];
     _str.toCharArray(_buf, 21);
     operationmodeSens.setValue(_buf);
}
void manualModeCallbackHandler(const IDatapoint& dp, DPValue value) {
     int _val = value.getU8();
     String _str =  manMode[_val];
     char _buf[12];
     _str.toCharArray(_buf, 12);
     manualmodeSens.setValue(_buf); 
     selectManualMode.setState(_val);    // setState(index)   
}


void tempRaumSollCallbackHandler(const IDatapoint& dp, DPValue value) {
     float _val = value.getFloat();
     RaumSollTempSens.setState(_val);
     HVACwaermepumpe.setTargetTemperature(_val);
}

void tempRaumSollRedCallbackHandler(const IDatapoint& dp, DPValue value) {
     RaumSollRedSens.setState(value.getFloat());
}

void tempWWsollCallbackHandler(const IDatapoint& dp, DPValue value) {
     WWtempSollSens.setState(value.getFloat());
}

void tempWWsoll2CallbackHandler(const IDatapoint& dp, DPValue value) {
     WWtempSoll2Sens.setState(value.getFloat());
}

void tempHystWWsollCallbackHandler(const IDatapoint& dp, DPValue value) {
     HystWWsollSens.setState(value.getFloat());
}

void tempHKniveauCallbackHandler(const IDatapoint& dp, DPValue value) {
     HKniveauSens.setState(value.getFloat());
}

void tempHKneigungCallbackHandler(const IDatapoint& dp, DPValue value) {
     HKneigungSens.setState(value.getFloat());
}


//*********************************************************************
void globalCallbackHandler(const IDatapoint& dp, DPValue value) {
  char value_str[15] = {0};
  value.getString(value_str, sizeof(value_str));
  // WebSerial.println(value_str);
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
    if (dbgSer) WebSerial.print("ID: ");
    if (dbgSer)  WebSerial.print(inputMessage1);
    if (dbgSer)  WebSerial.print(" - Set to: ");
    if (dbgSer)  WebSerial.println(inputMessage2);
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
  WebSerial.println(ssid);
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
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
    WiFi.begin(ssid, password);
  }
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    // WIFI is connected  
  }
  else {
    WebSerial.println("not reconnected!");
  }
}



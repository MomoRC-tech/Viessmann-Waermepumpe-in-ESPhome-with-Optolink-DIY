// home assistant integration ##########################################

#pragma once

#include <ArduinoHA.h>
#include "Vitocal_datapoints.h"
#include "Vitocal_polling.h"
extern volatile uint32_t vitoErrorThreshold; // from main sketch

// prefix to have unique IDs
#define HA_PREFIX "wp_bartels_"   

//*** forward declararions ***************************************************
void onMQTTConnected(void);
void onMQTTMessage(const char* topic, const uint8_t* payload, uint16_t length);
void setRaumSoll (HANumeric number, HANumber* sender);
void setRaumSollRed (HANumeric number, HANumber* sender);
void setWWSoll (HANumeric number, HANumber* sender);
void setWWSoll2 (HANumeric number, HANumber* sender);
void setHystWWsoll (HANumeric number, HANumber* sender);

void setHKniveau (HANumeric number, HANumber* sender);
void setHKneigung (HANumeric number, HANumber* sender);

void onTargetTemperatureCommand(HANumeric temperature, HAHVAC* sender);
void onPowerCommand(bool state, HAHVAC* sender);
void onModeCommand(HAHVAC::Mode mode, HAHVAC* sender);
void onManualModeCommand(int8_t index, HASelect* sender);

//*** sensor definitions ***************************************************
HASensorNumber RelEHeizStufeSens    (HA_PREFIX "EHeizstufe",        HANumber::PrecisionP0);   //working
HASensorNumber AussenTempSens       (HA_PREFIX "Aussentemperatur",  HANumber::PrecisionP1);   //working
HASensorNumber WWtempObenSens       (HA_PREFIX "WarmwasserOben",    HANumber::PrecisionP1);   //working
HASensorNumber VorlaufTempSetSens   (HA_PREFIX "VorlaufSoll",       HANumber::PrecisionP0);   //working
HASensorNumber VorlaufTempSens      (HA_PREFIX "Vorlauf",           HANumber::PrecisionP0);   //working
HASensorNumber RuecklaufTempSens    (HA_PREFIX "Ruecklauf",         HANumber::PrecisionP0);   //working

HABinarySensor heizkreispumpeSens       (HA_PREFIX "Heizkreispumpe");
HABinarySensor WWzirkulationspumpeSens  (HA_PREFIX "WWZirkulation");
HASensor       ventilHeizenWWSens       (HA_PREFIX "VentilHeizenWW");
HABinarySensor RelVerdichterSens        (HA_PREFIX "Verdichter");
HABinarySensor RelPrimaerquelleSens      (HA_PREFIX "Grundwasserpumpe");
HABinarySensor RelSekundaerPumpeSens    (HA_PREFIX "Sekundaerpumpe");

HABinarySensor Stoerung         (HA_PREFIX "WPStoerung");

HAHVAC HVACwaermepumpe(
  HA_PREFIX "Waermepumpe",
  HAHVAC::TargetTemperatureFeature | HAHVAC::PowerFeature | HAHVAC::ModesFeature
);

//*** set values ***************************************************
HANumber  WWtempSollSens       (HA_PREFIX "WarmwasserSoll",     HANumber::PrecisionP0);
HANumber  WWtempSoll2Sens      (HA_PREFIX "WarmwasserSoll2",    HANumber::PrecisionP0);
HANumber  RaumSollTempSens     (HA_PREFIX "Raumtemperatur",     HANumber::PrecisionP1);
HANumber  HystWWsollSens       (HA_PREFIX "HystereseWWsoll",    HANumber::PrecisionP1);

HANumber  HKneigungSens       (HA_PREFIX "NeigungHeizkennlinie",    HANumber::PrecisionP1);
HANumber  HKniveauSens        (HA_PREFIX "NiveauHeizkennlinie",    HANumber::PrecisionP1);

HANumber  RaumSollRedSens      (HA_PREFIX "RaumtemperaturRed",  HANumber::PrecisionP1);
HASensor  operationmodeSens    (HA_PREFIX "Betriebsmodus"); 
HASensor  manualmodeSens       (HA_PREFIX "ManualMode");
HASelect  selectManualMode     (HA_PREFIX "setManualMode");

HANumber fastPollInterval(HA_PREFIX "fastPollInterval");
HANumber mediumPollInterval(HA_PREFIX "mediumPollInterval");
HANumber slowPollInterval(HA_PREFIX "slowPollInterval");

// Diagnostics: error counters and threshold
HASensorNumber vitoErrorCountSens(HA_PREFIX "vito_error_count", HANumber::PrecisionP0);
HASensorNumber vitoConsecErrorSens(HA_PREFIX "vito_consecutive_errors", HANumber::PrecisionP0);
HANumber errorThresholdNumber(HA_PREFIX "vito_error_threshold", HANumber::PrecisionP0);

//###########################################################################
// setup home assistant integration##########################################
void setupHomeAssistant() {   
    //*** HA device ***************************************************
    device.setName(DEVICE_NAME);
    device.setSoftwareVersion(DEVICE_SWVERSION);
    device.setManufacturer(DEVICE_MANUFACTURER);
    device.setModel(DEVICE_MODEL); 
    device.enableSharedAvailability();
    device.enableLastWill();

    //*** setup sensors ***********************************************
    AussenTempSens.setIcon("mdi:home-thermometer-outline");     AussenTempSens.setName("Aussentemperatur");    AussenTempSens.setUnitOfMeasurement("C");
    WWtempObenSens.setIcon("mdi:bathtub");                      WWtempObenSens.setName("Warmwasser Oben");     WWtempObenSens.setUnitOfMeasurement("C");
    VorlaufTempSetSens.setIcon("mdi:thermometer-chevron-up");   VorlaufTempSetSens.setName("Vorlauf Soll");    VorlaufTempSetSens.setUnitOfMeasurement("C");  
    VorlaufTempSens.setIcon("mdi:thermometer-chevron-up");      VorlaufTempSens.setName("Vorlauf");            VorlaufTempSens.setUnitOfMeasurement("C");
    RuecklaufTempSens.setIcon("mdi:thermometer-chevron-down");  RuecklaufTempSens.setName("Ruecklauf");        RuecklaufTempSens.setUnitOfMeasurement("C");      

    heizkreispumpeSens.setIcon("mdi:pump");                  heizkreispumpeSens.setName("Heizkreispumpe");     
    WWzirkulationspumpeSens.setIcon("mdi:pump");             WWzirkulationspumpeSens.setName("WW Zirkulation");     
    ventilHeizenWWSens.setIcon("mdi:pipe-valve");             ventilHeizenWWSens.setName("Ventil Heizen-WW");   
    
    ventilHeizenWWSens.setForceUpdate(false);

    RelEHeizStufeSens.setIcon("mdi:radiator");                  RelEHeizStufeSens.setName("EHeizstufe");     
    RelVerdichterSens.setIcon("mdi:filter");                 RelVerdichterSens.setName("Verdichter");         
    RelPrimaerquelleSens.setIcon("mdi:pump");                RelPrimaerquelleSens.setName("Grundwasserpumpe");    
    RelSekundaerPumpeSens.setIcon("mdi:pump");              RelSekundaerPumpeSens.setName("Sekundaerpumpe"); 

    Stoerung.setIcon("mdi:alert-outline");                   Stoerung.setName("Stoerung");

    operationmodeSens.setIcon("mdi:state-machine");          operationmodeSens.setName("Modus"); 
    manualmodeSens.setIcon("mdi:braille");                   manualmodeSens.setName("Man.Modus"); 
    selectManualMode.setIcon("mdi:braille");                 selectManualMode.setName("set Man.Modus");
    selectManualMode.setOptions("normal;manuel;WW auf Temp2"); // use semicolons as separator of options
    selectManualMode.onCommand(onManualModeCommand); 

    WWtempSollSens.setIcon("mdi:state-machine");             WWtempSollSens.setName("Warmwasser Soll");             WWtempSollSens.setUnitOfMeasurement("C");         
    WWtempSollSens.setMin(20);  WWtempSollSens.setMax(60);   WWtempSollSens.setStep(1);                             WWtempSollSens.onCommand(setWWSoll);            WWtempSollSens.setMode(HANumber::ModeBox);  
    WWtempSoll2Sens.setIcon("mdi:state-machine");             WWtempSoll2Sens.setName("Warmwasser Soll2");             WWtempSoll2Sens.setUnitOfMeasurement("C");         
    WWtempSoll2Sens.setMin(20);  WWtempSoll2Sens.setMax(60);   WWtempSoll2Sens.setStep(1);                             WWtempSoll2Sens.onCommand(setWWSoll2);       WWtempSoll2Sens.setMode(HANumber::ModeBox);

    HKneigungSens.setIcon("mdi:chart-bell-curve-cumulative");             HKneigungSens.setName("Neigung Heizkennlinie");       HKneigungSens.setUnitOfMeasurement("-");         
    HKneigungSens.setMin(0);  HKneigungSens.setMax(1);      HKneigungSens.setStep(0.1);                           HKneigungSens.onCommand(setHKneigung);
    HKneigungSens.setMode(HANumber::ModeBox);
    
    HKniveauSens.setIcon("mdi:chart-bell-curve-cumulative");              HKniveauSens.setName("Niveau Heizkennlinie");         HKniveauSens.setUnitOfMeasurement("K");         
    HKniveauSens.setMin(0);   HKniveauSens.setMax(10);     HKniveauSens.setStep(0.1);                             HKniveauSens.onCommand(setHKniveau);
    HKniveauSens.setMode(HANumber::ModeBox);

    HystWWsollSens.setIcon("mdi:state-machine");               HystWWsollSens.setName("Hysterese WW Soll");       HystWWsollSens.setUnitOfMeasurement("C"); 
    HystWWsollSens.setMin(1);  HystWWsollSens.setMax(20);      HystWWsollSens.setStep(0.5);                       HystWWsollSens.onCommand(setHystWWsoll); 
    HystWWsollSens.setMode(HANumber::ModeBox);

    RaumSollTempSens.setIcon("mdi:state-machine");               RaumSollTempSens.setName("Raumtemperatur Soll");     RaumSollTempSens.setUnitOfMeasurement("C"); 
    RaumSollTempSens.setMin(10);  RaumSollTempSens.setMax(30);   RaumSollTempSens.setStep(0.5);                       RaumSollTempSens.onCommand(setRaumSoll);    
    RaumSollTempSens.setMode(HANumber::ModeBox);

    RaumSollRedSens.setIcon("mdi:state-machine");                RaumSollRedSens.setName("Raumtemperatur Red. Soll");     RaumSollRedSens.setUnitOfMeasurement("C");  
    RaumSollRedSens.setMin(10);   RaumSollRedSens.setMax(30);    RaumSollRedSens.setStep(0.5);                            RaumSollRedSens.onCommand(setRaumSollRed);  
    RaumSollRedSens.setMode(HANumber::ModeBox);

    // polling interval controls (seconds) - allow tuning from Home Assistant
    fastPollInterval.setIcon("mdi:timer-sand");
    fastPollInterval.setName("Vito Fast Poll Interval");
    fastPollInterval.setUnitOfMeasurement("s");
    fastPollInterval.setMin(5);
    fastPollInterval.setMax(300);
    fastPollInterval.setStep(1);
    fastPollInterval.setMode(HANumber::ModeBox);  
    fastPollInterval.setRetain(true);  // keep value across broker restarts
    fastPollInterval.onCommand([](HANumeric number, HANumber* sender) {
        if (!number.isSet() || sender == nullptr) {
            return;
        }
        float requested = number.toFloat();
        float applied = requested < 5.0f ? 5.0f : requested;
        vitoFastState.intervalMs = (uint32_t)(applied * 1000.0f);
        sender->setState(applied);
    });

    mediumPollInterval.setIcon("mdi:timer-sand-half");
    mediumPollInterval.setName("Vito Medium Poll Interval");
    mediumPollInterval.setUnitOfMeasurement("s");
    mediumPollInterval.setMin(5);
    mediumPollInterval.setMax(600);
    mediumPollInterval.setStep(1);
    mediumPollInterval.setMode(HANumber::ModeBox); 
    mediumPollInterval.setRetain(true);
    mediumPollInterval.onCommand([](HANumeric number, HANumber* sender) {
        if (!number.isSet() || sender == nullptr) {
            return;
        }
        float requested = number.toFloat();
        float applied = requested < 5.0f ? 5.0f : requested;
        vitoMediumState.intervalMs = (uint32_t)(applied * 1000.0f);
        sender->setState(applied);
    });

    slowPollInterval.setIcon("mdi:timer-sand-complete");
    slowPollInterval.setName("Vito Slow Poll Interval");
    slowPollInterval.setUnitOfMeasurement("s");
    slowPollInterval.setMin(5);
    slowPollInterval.setMax(1800);
    slowPollInterval.setStep(1);
    slowPollInterval.setMode(HANumber::ModeBox); 
    slowPollInterval.setRetain(true);
    slowPollInterval.onCommand([](HANumeric number, HANumber* sender) {
        if (!number.isSet() || sender == nullptr) {
            return;
        }
        float requested = number.toFloat();
        float applied = requested < 5.0f ? 5.0f : requested;
        vitoSlowState.intervalMs = (uint32_t)(applied * 1000.0f);
        sender->setState(applied);
    });

    HVACwaermepumpe.setName("Waermepumpe");                                       
    HVACwaermepumpe.setMinTemp(10);
    HVACwaermepumpe.setMaxTemp(30);
    HVACwaermepumpe.setTempStep(0.5);
    HVACwaermepumpe.setModes(HAHVAC::OffMode | HAHVAC::HeatMode  | HAHVAC::CoolMode);
    HVACwaermepumpe.onTargetTemperatureCommand(onTargetTemperatureCommand);
    HVACwaermepumpe.onPowerCommand(onPowerCommand);
    HVACwaermepumpe.onModeCommand(onModeCommand);    
    
    //*** setup MQTT ***********************************************
    //mqtt.onMessage(onMQTTMessage);
    mqtt.onConnected(onMQTTConnected);
    mqtt.setDataPrefix(MQTT_DATAPREFIX);
    mqtt.setDiscoveryPrefix(MQTT_DISCOVERYPREFIX);
    mqtt.begin(BROKER_ADDR, BROKER_PORT, BROKER_USERNAME, BROKER_PASSWORD);

    // publish default polling intervals so HA sees initial state (seconds)
    fastPollInterval.setState((float)(vitoFastState.intervalMs / 1000UL));
    mediumPollInterval.setState((float)(vitoMediumState.intervalMs / 1000UL));
    slowPollInterval.setState((float)(vitoSlowState.intervalMs / 1000UL));
    

    // diagnostics setup
    vitoErrorCountSens.setIcon("mdi:counter");
    vitoErrorCountSens.setName("VitoWiFi Error Count");
    vitoConsecErrorSens.setIcon("mdi:counter");
    vitoConsecErrorSens.setName("VitoWiFi Consecutive Errors");

    errorThresholdNumber.setIcon("mdi:alert-decagram-outline");
    errorThresholdNumber.setName("VitoWiFi Error Threshold");
    errorThresholdNumber.setUnitOfMeasurement("");
    errorThresholdNumber.setMin(1);
    errorThresholdNumber.setMax(100);
    errorThresholdNumber.setStep(1);
    errorThresholdNumber.setMode(HANumber::ModeBox);  
    errorThresholdNumber.setRetain(true);
    errorThresholdNumber.onCommand([](HANumeric value, HANumber* sender) {
        if (!value.isSet() || sender == nullptr) {
            return;
        }
        int v = (int)value.toFloat();
        if (v < 1) v = 1;
        if (v > 100) v = 100;
        vitoErrorThreshold = (uint32_t)v;
        sender->setState((float)v);
    });
    errorThresholdNumber.setState((float)vitoErrorThreshold);
}


// VitoWiFi v3 instance and datapoints (defined elsewhere)
extern VitoWiFi::VitoWiFi<VitoWiFi::VS1> vitoWIFI;
extern VitoWiFi::Datapoint setTempRaumSoll;
extern VitoWiFi::Datapoint setTempRaumSollRed;
extern VitoWiFi::Datapoint setTempHystWWsoll;
extern VitoWiFi::Datapoint setTempHKneigung;
extern VitoWiFi::Datapoint setTempHKniveau;
extern VitoWiFi::Datapoint setTempWWsoll;
extern VitoWiFi::Datapoint setTempWWsoll2;
extern VitoWiFi::Datapoint setManualMode;

void setRaumSoll (HANumeric number, HANumber* sender) {
    if (number.isSet()) {
        float val = number.toFloat();
        vitoWIFI.write(setTempRaumSoll, val);
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setRaumSollRed (HANumeric number, HANumber* sender) {
    if (number.isSet()) {
        float val = number.toFloat();
        vitoWIFI.write(setTempRaumSollRed, val);
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setHystWWsoll (HANumeric number, HANumber* sender) {
    if (number.isSet()) {
        float val = number.toFloat();
        vitoWIFI.write(setTempHystWWsoll, val);
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setHKneigung (HANumeric number, HANumber* sender) {
    if (number.isSet()) {
        float val = number.toFloat();
        vitoWIFI.write(setTempHKneigung, val);
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setHKniveau (HANumeric number, HANumber* sender) {
    if (number.isSet()) {
        float val = number.toFloat();
        vitoWIFI.write(setTempHKniveau, val);
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setWWSoll (HANumeric number, HANumber* sender) {
    if (number.isSet()) {
        float val = number.toFloat();
        vitoWIFI.write(setTempWWsoll, val);
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setWWSoll2 (HANumeric number, HANumber* sender) {
    if (number.isSet()) {
        float val = number.toFloat();
        vitoWIFI.write(setTempWWsoll2, val);
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void onTargetTemperatureCommand(HANumeric temperature, HAHVAC* sender) {
    float val = temperature.toFloat();
    vitoWIFI.write(setTempRaumSoll, val);

    sender->setTargetTemperature(temperature); // report target temperature back to the HA panel
}

void onPowerCommand(bool state, HAHVAC* sender) {
  if (state) {
    Serial.println("Power on");
  } else {
    Serial.println("Power off");
  }
}

void onModeCommand(HAHVAC::Mode mode, HAHVAC* sender) {
    if (mode == HAHVAC::OffMode) {
        Serial.println("off");
    } else if (mode == HAHVAC::AutoMode) {
        Serial.println("auto");
    } else if (mode == HAHVAC::CoolMode) {
        Serial.println("cool");
    } else if (mode == HAHVAC::HeatMode) {
        Serial.println("heat");
    } else if (mode == HAHVAC::DryMode) {
        Serial.println("dry");
    } else if (mode == HAHVAC::FanOnlyMode) {
        Serial.println("fan only");
    }

    sender->setMode(mode); // report mode back to the HA panel
}

void onManualModeCommand(int8_t index, HASelect* sender)
{
    switch (index) {
    case 0:
        // Option "Normal" was selected
        vitoWIFI.write(setManualMode, static_cast<uint8_t>(index));
        break;

    case 1:
        // Option "Manueller Heizbetrieb" was selected
        vitoWIFI.write(setManualMode, static_cast<uint8_t>(index));
        break;

    case 2:
        // Option "1x WW auf Temp2" was selected
        vitoWIFI.write(setManualMode, static_cast<uint8_t>(index));
        break;

    default:
        // unknown option
        return;
    }

    sender->setState(index); // report the selected option back to the HA panel
}

void onMQTTMessage(const char* topic, const uint8_t* payload, uint16_t length) {
    // this method will be called each time the device receives an MQTT message
}

void onMQTTConnected() {
    // this method will be called when connection to MQTT broker is established
    device.publishAvailability();
}

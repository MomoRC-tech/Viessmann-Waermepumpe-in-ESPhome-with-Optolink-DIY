// home assistant integration ##########################################

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
HASensorNumber RelEHeizStufeSens    ("EHeizstufe",        HANumber::PrecisionP0);   //working
HASensorNumber AussenTempSens       ("Aussentemperatur",  HANumber::PrecisionP1);   //working
HASensorNumber WWtempObenSens       ("WarmwasserOben",    HANumber::PrecisionP1);   //working
HASensorNumber VorlaufTempSetSens   ("VorlaufSoll",       HANumber::PrecisionP0);   //working
HASensorNumber VorlaufTempSens      ("Vorlauf",           HANumber::PrecisionP0);   //working
HASensorNumber RuecklaufTempSens    ("Ruecklauf",         HANumber::PrecisionP0);   //working
HASensorNumber CompFrequencySens    ("CompFrequency",     HANumber::PrecisionP0);   //new

HABinarySensor heizkreispumpeSens       ("Heizkreispumpe");
HABinarySensor WWzirkulationspumpeSens  ("WWZirkulation");
HASensor       ventilHeizenWWSens       ("VentilHeizenWW");
HABinarySensor RelVerdichterSens        ("Verdichter");
HABinarySensor RelPrimaerquelleSens      ("Grundwasserpumpe");
HABinarySensor RelSekundaerPumpeSens    ("Sekundaerpumpe");

HABinarySensor Stoerung         ("WPStoerung");

HAHVAC HVACwaermepumpe(
  "Waermepumpe",
  HAHVAC::TargetTemperatureFeature | HAHVAC::PowerFeature | HAHVAC::ModesFeature
);

//*** set values ***************************************************
HANumber  WWtempSollSens       ("WarmwasserSoll",     HANumber::PrecisionP0);
HANumber  WWtempSoll2Sens      ("WarmwasserSoll2",    HANumber::PrecisionP0);
HANumber  RaumSollTempSens     ("Raumtemperatur",     HANumber::PrecisionP1);
HANumber  HystWWsollSens       ("HystereseWWsoll",    HANumber::PrecisionP1);

HANumber  HKneigungSens       ("NeigungHeizkennlinie",    HANumber::PrecisionP1);
HANumber  HKniveauSens        ("NiveauHeizkennlinie",    HANumber::PrecisionP1);

HANumber  RaumSollRedSens      ("RaumtemperaturRed",  HANumber::PrecisionP1);
HASensor  operationmodeSens    ("Betriebsmodus"); 
HASensor  manualmodeSens       ("ManualMode");
HASelect  selectManualMode     ("setManualMode");

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
    CompFrequencySens.setIcon("mdi:sine-wave");                 CompFrequencySens.setName("Compressor Frequency");        

    heizkreispumpeSens.setIcon("mdi:pump");                  heizkreispumpeSens.setName("Heizkreispumpe");     
    WWzirkulationspumpeSens.setIcon("mdi:pump");             WWzirkulationspumpeSens.setName("WW Zirkulation");     
    ventilHeizenWWSens.setIcon("mdi:pipe-valve");             ventilHeizenWWSens.setName("Ventil Heizen-WW");   
    
    ventilHeizenWWSens.setForceUpdate(true);

    RelEHeizStufeSens.setIcon("mdi:radiator");                  RelEHeizStufeSens.setName("EHeizstufe");     
    RelVerdichterSens.setIcon("mdi:filter");                 RelVerdichterSens.setName("Verdichter");         
    RelPrimaerquelleSens.setIcon("mdi:pump");                RelPrimaerquelleSens.setName("Grundwasserpumpe");    
    RelSekundaerPumpeSens.setIcon("mdi:pump");              RelSekundaerPumpeSens.setName("Sekundaerpumpe"); 

    Stoerung.setIcon("mdi:alert-outline");                   Stoerung.setName("Stoerung");

    operationmodeSens.setIcon("mdi:state-machine");          operationmodeSens.setName("Modus"); 
    manualmodeSens.setIcon("mdi:braille");                   manualmodeSens.setName("Man.Modus"); 
    selectManualMode.setIcon("mdi:braille");                 selectManualMode.setName("set Man.Modus");
    selectManualMode.setOptions("Normal;man.Heizbetrieb;1x WW auf Temp2"); // use semicolons as separator of options
    selectManualMode.onCommand(onManualModeCommand); 

    WWtempSollSens.setIcon("mdi:state-machine");             WWtempSollSens.setName("Warmwasser Soll");             WWtempSollSens.setUnitOfMeasurement("C");         
    WWtempSollSens.setMin(20);  WWtempSollSens.setMax(60);   WWtempSollSens.setStep(1);                             WWtempSollSens.onCommand(setWWSoll);
    WWtempSoll2Sens.setIcon("mdi:state-machine");             WWtempSoll2Sens.setName("Warmwasser Soll2");             WWtempSoll2Sens.setUnitOfMeasurement("C");         
    WWtempSoll2Sens.setMin(20);  WWtempSoll2Sens.setMax(60);   WWtempSoll2Sens.setStep(1);                             WWtempSoll2Sens.onCommand(setWWSoll2);

    HKneigungSens.setIcon("mdi:chart-bell-curve-cumulative");             HKneigungSens.setName("Neigung Heizkennlinie");       HKneigungSens.setUnitOfMeasurement("-");         
    HKneigungSens.setMin(0);  HKneigungSens.setMax(1);      HKneigungSens.setStep(0.1);                           HKneigungSens.onCommand(setHKneigung);
    
    HKniveauSens.setIcon("mdi:chart-bell-curve-cumulative");              HKniveauSens.setName("Niveau Heizkennlinie");         HKniveauSens.setUnitOfMeasurement("K");         
    HKniveauSens.setMin(0);   HKniveauSens.setMax(10);     HKniveauSens.setStep(0.1);                             HKniveauSens.onCommand(setHKniveau);

    HystWWsollSens.setIcon("mdi:state-machine");               HystWWsollSens.setName("Hysterese WW Soll");       HystWWsollSens.setUnitOfMeasurement("C"); 
    HystWWsollSens.setMin(1);  HystWWsollSens.setMax(20);      HystWWsollSens.setStep(0.5);                       HystWWsollSens.onCommand(setHystWWsoll); 

    RaumSollTempSens.setIcon("mdi:state-machine");               RaumSollTempSens.setName("Raumtemperatur Soll");     RaumSollTempSens.setUnitOfMeasurement("C"); 
    RaumSollTempSens.setMin(10);  RaumSollTempSens.setMax(30);   RaumSollTempSens.setStep(0.5);                       RaumSollTempSens.onCommand(setRaumSoll);    
    
    RaumSollRedSens.setIcon("mdi:state-machine");                RaumSollRedSens.setName("Raumtemperatur Red. Soll");     RaumSollRedSens.setUnitOfMeasurement("C");  
    RaumSollRedSens.setMin(10);   RaumSollRedSens.setMax(30);    RaumSollRedSens.setStep(0.5);                            RaumSollRedSens.onCommand(setRaumSollRed);  

    HVACwaermepumpe.setName("Waermepumpe");                                       
    HVACwaermepumpe.setMinTemp(10);  HVACwaermepumpe.setMaxTemp(30);    HVACwaermepumpe.setTempStep(0.5);   
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
}


void setRaumSoll (HANumeric number, HANumber* sender) {
    DPValue _value(number.toFloat());
    if (!number.isSet()) {
        // the reset command was send by Home Assistant
    } else {
        VitoWiFi.writeDatapoint(setTempRaumSoll, _value);      
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setRaumSollRed (HANumeric number, HANumber* sender) {
    DPValue _value(number.toFloat());
    if (!number.isSet()) {
        // the reset command was send by Home Assistant
    } else {
        VitoWiFi.writeDatapoint(setTempRaumSollRed, _value);      
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setHystWWsoll (HANumeric number, HANumber* sender) {
    DPValue _value(number.toFloat());
    if (!number.isSet()) {
        // the reset command was send by Home Assistant
    } else {
        VitoWiFi.writeDatapoint(setTempHystWWsoll, _value);      
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setHKneigung (HANumeric number, HANumber* sender) {
    DPValue _value(number.toFloat());
    if (!number.isSet()) {
        // the reset command was send by Home Assistant
    } else {
        VitoWiFi.writeDatapoint(setTempHKneigung, _value);      
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setHKniveau (HANumeric number, HANumber* sender) {
    DPValue _value(number.toFloat());
    if (!number.isSet()) {
        // the reset command was send by Home Assistant
    } else {
        VitoWiFi.writeDatapoint(setTempHKniveau, _value);      
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setWWSoll (HANumeric number, HANumber* sender) {
    DPValue _value(number.toFloat());
    if (!number.isSet()) {
        // the reset command was send by Home Assistant
    } else {
        VitoWiFi.writeDatapoint(setTempWWsoll, _value);      
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void setWWSoll2 (HANumeric number, HANumber* sender) {
    DPValue _value(number.toFloat());
    if (!number.isSet()) {
        // the reset command was send by Home Assistant
    } else {
        VitoWiFi.writeDatapoint(setTempWWsoll2, _value);      
    }
    sender->setState(number); // report the selected option back to the HA panel
}

void onTargetTemperatureCommand(HANumeric temperature, HAHVAC* sender) {
    //float temperatureFloat = temperature.toFloat();
    DPValue _value(temperature.toFloat());
    VitoWiFi.writeDatapoint(setTempRaumSoll, _value);  

    sender->setTargetTemperature(temperature); // report target temperature back to the HA panel
}

void onPowerCommand(bool state, HAHVAC* sender) {
  if (state) {
    WebSerial.println("Power on");
  } else {
    WebSerial.println("Power off");
  }
}

void onModeCommand(HAHVAC::Mode mode, HAHVAC* sender) {
    if (mode == HAHVAC::OffMode) {
        WebSerial.println("off");
    } else if (mode == HAHVAC::AutoMode) {
        WebSerial.println("auto");
    } else if (mode == HAHVAC::CoolMode) {
        WebSerial.println("cool");
    } else if (mode == HAHVAC::HeatMode) {
        WebSerial.println("heat");
    } else if (mode == HAHVAC::DryMode) {
        WebSerial.println("dry");
    } else if (mode == HAHVAC::FanOnlyMode) {
        WebSerial.println("fan only");
    }

    sender->setMode(mode); // report mode back to the HA panel
}

void onManualModeCommand(int8_t index, HASelect* sender)
{
    DPValue _idx((uint8_t)index);
    switch (index) {
    case 0:
        // Option "Normal" was selected
        VitoWiFi.writeDatapoint(setManualMode, _idx);
        break;

    case 1:
        // Option "Manueller Heizbetrieb" was selected
        VitoWiFi.writeDatapoint(setManualMode, _idx);
        break;

    case 2:
        // Option "1x WW auf Temp2" was selected
        VitoWiFi.writeDatapoint(setManualMode, _idx);
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

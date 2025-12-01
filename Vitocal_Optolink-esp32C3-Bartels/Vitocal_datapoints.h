#pragma once

#include <VitoWiFi.h>

// Temperatures (2 bytes, div10 -> float)
VitoWiFi::Datapoint dpTempOutside    ("AussenTemp",     0x0101, 2, VitoWiFi::div10);
VitoWiFi::Datapoint dpWWoben        ("WWtempOben",    0x010D, 2, VitoWiFi::div10);
VitoWiFi::Datapoint dpVorlaufSoll   ("VorlaufTempSet",0x1800, 2, VitoWiFi::div10);
VitoWiFi::Datapoint dpVorlaufIst    ("VorlaufTemp",   0x0105, 2, VitoWiFi::div10);
VitoWiFi::Datapoint dpRuecklauf     ("RuecklaufTemp", 0x0106, 2, VitoWiFi::div10);

// Compressor frequency (mode / short temp)
VitoWiFi::Datapoint dpCompFrequency ("compressorFreq", 0x1A54, 1, VitoWiFi::noconv);

// Raum / WW set + get
VitoWiFi::Datapoint dpTempRaumSoll      ("RaumSollTemp",     0x2000, 2, VitoWiFi::div10);
VitoWiFi::Datapoint setTempRaumSoll     ("SetRaumSollTemp",  0x2000, 2, VitoWiFi::div10);
VitoWiFi::Datapoint dpTempRaumSollRed   ("RaumSollRed",      0x2001, 2, VitoWiFi::div10);
VitoWiFi::Datapoint setTempRaumSollRed  ("SetRaumSollRed",   0x2001, 2, VitoWiFi::div10);

VitoWiFi::Datapoint dpTempWWSoll        ("WWtempSoll",       0x6000, 2, VitoWiFi::div10);
VitoWiFi::Datapoint setTempWWsoll       ("SetWWtempSoll",    0x6000, 2, VitoWiFi::div10);
VitoWiFi::Datapoint dpTempWWSoll2       ("WWtempSoll2",      0x600C, 2, VitoWiFi::div10);
VitoWiFi::Datapoint setTempWWsoll2      ("SetWWtempSoll2",   0x600C, 2, VitoWiFi::div10);

VitoWiFi::Datapoint dpTempHystWWSoll    ("HystWWsoll",       0x6007, 2, VitoWiFi::div10);
VitoWiFi::Datapoint setTempHystWWsoll   ("SetTempHystWWsoll",0x6007, 2, VitoWiFi::div10);

VitoWiFi::Datapoint dpTempHKniveau      ("HKniveau",         0x2006, 2, VitoWiFi::div10);
VitoWiFi::Datapoint setTempHKniveau     ("SetHKniveau",      0x2006, 2, VitoWiFi::div10);
VitoWiFi::Datapoint dpTempHKNeigung     ("HKneigung",        0x2007, 2, VitoWiFi::div10);
VitoWiFi::Datapoint setTempHKneigung    ("SetHKneigung",     0x2007, 2, VitoWiFi::div10);

// Status / modes (1 byte noconv -> uint8_t)
VitoWiFi::Datapoint dpOperationMode     ("operationmode",    0xB000, 1, VitoWiFi::noconv);
VitoWiFi::Datapoint setOperationMode    ("setOperationmode", 0xB000, 1, VitoWiFi::noconv);
VitoWiFi::Datapoint dpManualMode        ("manualmode",       0xB020, 1, VitoWiFi::noconv);
VitoWiFi::Datapoint setManualMode       ("setManualmode",    0xB020, 1, VitoWiFi::noconv);

// Relays
VitoWiFi::Datapoint dpHeizkreispumpe    ("heizkreispumpe",       0x048D, 1, VitoWiFi::noconv);
VitoWiFi::Datapoint dpWWZirkPumpe       ("WWzirkulationspumpe",  0x0490, 1, VitoWiFi::noconv);
VitoWiFi::Datapoint dpVentilHeizenWW    ("ventilHeizenWW",       0x0494, 1, VitoWiFi::noconv);
VitoWiFi::Datapoint dpRelVerdichter     ("RelVerdichter",        0x0480, 1, VitoWiFi::noconv);
VitoWiFi::Datapoint dpRelPrimaerquelle  ("RelPrimärquelle",      0x0482, 1, VitoWiFi::noconv);
VitoWiFi::Datapoint dpRelSekundaerPumpe ("RelSekundaerPumpe",    0x0484, 1, VitoWiFi::noconv);
VitoWiFi::Datapoint dpRelEHeizStufe1    ("RelEHeizStufe1",       0x0488, 1, VitoWiFi::noconv);
VitoWiFi::Datapoint dpRelEHeizStufe2    ("RelEHeizStufe2",       0x0489, 1, VitoWiFi::noconv);

VitoWiFi::Datapoint dpStoerung          ("stoerung",             0x0491, 1, VitoWiFi::noconv);

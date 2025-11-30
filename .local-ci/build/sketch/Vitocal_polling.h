#line 1 "C:\\Users\\Momo\\Documents\\30_Coding\\01_github local\\Viessmann-Waermepumpe-in-ESPhome-with-Optolink-DIY\\.local-ci\\Vitocal_basic-esp8266-Bartels\\Vitocal_polling.h"
#pragma once

#include <stdint.h>

struct VitoPollGroupState {
  uint8_t index;            // next datapoint index within the group
  uint32_t lastRequestMs;   // time of the last successful read() queue
  uint32_t lastRoundEndMs;  // time when the most recent round completed
  uint32_t intervalMs;      // minimum delay between full rounds
};

extern VitoPollGroupState vitoFastState;
extern VitoPollGroupState vitoMediumState;
extern VitoPollGroupState vitoSlowState;

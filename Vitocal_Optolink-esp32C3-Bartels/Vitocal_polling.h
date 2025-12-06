#pragma once

#include <stdint.h>

struct VitoPollGroupState {
  uint8_t index;            // next datapoint index within the group
  uint32_t lastRequestMs;   // time of the last successful read() queue
  uint32_t  lastRoundEndMs; // we now treat this as "lastRoundStartMs"
  uint32_t intervalMs;      // minimum delay between full rounds
};

extern VitoPollGroupState vitoFastState;
extern VitoPollGroupState vitoMediumState;
extern VitoPollGroupState vitoSlowState;

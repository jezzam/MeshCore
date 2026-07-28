#include <Arduino.h>
#include <helpers/ArduinoHelpers.h>
#include "target.h"

LinuxBoard board;
PktFwdRadio radio_driver;
LinuxRTCClock rtc_clock;
SensorManager sensors;

bool radio_init() {
  radio_driver.begin();
  return true;
}

mesh::LocalIdentity radio_new_identity() {
  // StdRNG (src/helpers/ArduinoHelpers.h) uses the process-global random()/
  // srandom() state, already seeded with real entropy by the time this runs
  // (main.cpp calls fast_rng.begin(radio_driver.getRngSeed()) before any
  // identity generation) - no radio-noise entropy trick needed on Linux.
  StdRNG rng;
  return mesh::LocalIdentity(&rng);
}

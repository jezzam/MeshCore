#pragma once

#include "LinuxBoard.h"
#include "LinuxRTCClock.h"
#include "PktFwdRadio.h"
#include <helpers/SensorManager.h>

extern LinuxBoard board;
extern PktFwdRadio radio_driver;
extern LinuxRTCClock rtc_clock;
extern SensorManager sensors;

bool radio_init();
mesh::LocalIdentity radio_new_identity();

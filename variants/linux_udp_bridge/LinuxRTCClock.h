#pragma once

#include <MeshCore.h>
#include <ctime>

// Real wall-clock time - a genuine improvement over embedded fallbacks like
// VolatileRTCClock (which just accumulates millis() from an arbitrary base
// time), since the Bobcat has a real, correct system clock.
class LinuxRTCClock : public mesh::RTCClock {
public:
  uint32_t getCurrentTime() override { return (uint32_t)time(nullptr); }
  void setCurrentTime(uint32_t) override { /* no-op: system clock is authoritative */ }
};

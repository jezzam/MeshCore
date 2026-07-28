#pragma once

#include <MeshCore.h>
#include <cstdlib>

// Minimal MainBoard for a Docker container on a mains-powered Linux host -
// no battery, no display, no GPIO, no BLE, no button. Only overrides the
// pure-virtuals (MainBoard's other methods already have sensible no-op
// defaults - src/MeshCore.h).
class LinuxBoard : public mesh::MainBoard {
public:
  void begin() { }

  uint16_t getBattMilliVolts() override { return 5000; }  // mains-powered, report "always full"
  const char* getManufacturerName() const override { return "Bobcat Miner 300 (repurposed, linux_udp_bridge)"; }

  void reboot() override {
    // No point trying to reboot the host from inside a container - let
    // Docker's restart policy relaunch us instead (see sd-image/README.md
    // style reasoning: fail fast and cleanly rather than half-recover).
    exit(1);
  }

  uint8_t getStartupReason() const override { return BD_STARTUP_NORMAL; }

  void onBootComplete() override { }
};

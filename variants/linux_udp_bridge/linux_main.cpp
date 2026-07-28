// Arduino cores normally provide main() themselves (call setup() once, then
// loop() forever). On Linux we provide that ourselves. A small sleep between
// iterations avoids pinning a CPU core at 100% - the embedded loop() body is
// non-blocking by design (per-cycle polling), which is fine on a microcontroller
// but wasteful on a general-purpose OS running this 24/7.
#include <ctime>

extern void setup();
extern void loop();

int main() {
  setup();
  struct timespec ts = {0, 2 * 1000000L};  // 2ms
  for (;;) {
    loop();
    nanosleep(&ts, nullptr);
  }
  return 0;
}

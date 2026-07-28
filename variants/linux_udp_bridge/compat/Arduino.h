#pragma once

// Minimal Arduino.h compatibility shim for the LINUX_PLATFORM build.
// Just enough for src/, examples/simple_repeater/, and src/helpers/ to
// compile and behave correctly on Linux - not a general Arduino core clone.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/select.h>

#include "Stream.h"

typedef uint8_t byte;

#define PROGMEM
#define F(s) (s)

// Real Arduino cores define these as blind macros, which works for mixed-type
// comparisons (e.g. this codebase calls min(long, int), which std::min's
// exact-type template deduction rejects outright) but is a well-known source
// of collisions with STL headers that use std::min/max internally (the
// classic "Arduino/windows.h min/max problem"). Since this build uses real
// STL (std::string in FS.h, a JSON library later), use two-independent-
// template-parameter global functions instead of macros - same mixed-type
// flexibility, zero risk of corrupting "std::min(...)" text in STL headers.
template <typename A, typename B>
inline auto min(A a, B b) -> decltype(a < b ? a : b) { return a < b ? a : b; }
template <typename A, typename B>
inline auto max(A a, B b) -> decltype(a > b ? a : b) { return a > b ? a : b; }
#define constrain(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))

inline unsigned long millis() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}

inline unsigned long micros() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)(ts.tv_sec * 1000000UL + ts.tv_nsec / 1000UL);
}

inline void delay(unsigned long ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, nullptr);
}

// random()/randomSeed(): match Arduino's semantics (random(min,max) exclusive of max)
// but seed from real entropy at startup so StdRNG (src/helpers/ArduinoHelpers.h) gets
// good entropy for free, without needing a Linux-specific RNG class.
// ltoa(): available in Arduino cores (and BSD/avr-libc) but not in glibc.
inline char* ltoa(long value, char* buf, int base) {
  if (base == 10) { snprintf(buf, 24, "%ld", value); return buf; }
  // only base 10 is actually used in this codebase (src/helpers/TxtDataHelpers.cpp); a
  // minimal fallback for other bases just in case, not a full itoa implementation.
  unsigned long uv = value < 0 ? (unsigned long)(-value) : (unsigned long)value;
  char tmp[32]; int i = 0;
  if (uv == 0) tmp[i++] = '0';
  while (uv > 0) { int d = uv % base; tmp[i++] = d < 10 ? '0' + d : 'a' + d - 10; uv /= base; }
  int j = 0;
  if (value < 0 && base == 10) buf[j++] = '-';
  while (i > 0) buf[j++] = tmp[--i];
  buf[j] = 0;
  return buf;
}

inline void randomSeed(long seed) { srandom((unsigned)seed); }
inline long random(long howbig) { return howbig <= 0 ? 0 : random() % howbig; }
inline long random(long howsmall, long howbig) { return howsmall >= howbig ? howsmall : howsmall + random(howbig - howsmall); }

// A SerialStream wraps stdout (all Serial.print/println/printf output) and, if
// stdin is a TTY or explicitly piped (e.g. `docker attach`), stdin for the admin
// CLI - matching a real device's USB-serial console experience.
class SerialStream : public Stream {
public:
  size_t write(uint8_t c) override { return ::write(STDOUT_FILENO, &c, 1) == 1 ? 1 : 0; }
  size_t write(const uint8_t* buf, size_t len) override {
    size_t total = 0;
    while (total < len) {
      ssize_t n = ::write(STDOUT_FILENO, buf + total, len - total);
      if (n <= 0) break;
      total += (size_t)n;
    }
    return total;
  }
  int available() override {
    // Once stdin has hit EOF (the normal case for a non-interactive
    // `docker run` without -it - select() reports a closed/EOF stdin as
    // "readable" forever, which without this check causes read() to spin
    // returning -1 forever, corrupted to 0xFF once truncated to char by
    // main.cpp's command-buffer loop, busy-looping "Unknown command" at
    // 100% CPU indefinitely. Caught by an actual smoke-test run.
    if (_eof) return 0;
    fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0};
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0 ? 1 : 0;
  }
  int read() override {
    uint8_t c;
    ssize_t n = ::read(STDIN_FILENO, &c, 1);
    if (n == 0) { _eof = true; return -1; }
    return n == 1 ? c : -1;
  }
  void begin(long) { }

private:
  bool _eof = false;
};

extern SerialStream Serial;

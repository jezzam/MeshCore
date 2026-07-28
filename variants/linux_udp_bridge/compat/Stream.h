#pragma once

// Minimal Arduino Stream.h compatibility shim for the LINUX_PLATFORM build.
// Scope matches actual usage (verified by grep): available/read/readBytes,
// plus everything Print already provides.

#include "Print.h"

class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() { return -1; }  // not needed by anything we compile; default matches "unsupported"

  // Returns number of bytes actually read (matches Arduino's Stream::readBytes contract,
  // which src/Identity.cpp relies on for exact-length comparisons).
  virtual size_t readBytes(uint8_t* buf, size_t len) {
    size_t n = 0;
    while (n < len) {
      int c = read();
      if (c < 0) break;
      buf[n++] = (uint8_t)c;
    }
    return n;
  }
  size_t readBytes(char* buf, size_t len) { return readBytes((uint8_t*)buf, len); }
};

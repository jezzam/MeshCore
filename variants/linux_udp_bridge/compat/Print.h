#pragma once

// Minimal Arduino Print.h compatibility shim for the LINUX_PLATFORM build.
// Scope is deliberately narrow: only the overloads actually called across
// src/, examples/simple_repeater/, and src/helpers/ (verified by grep before
// writing this), not a general-purpose Arduino Print reimplementation.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

class Print {
public:
  virtual size_t write(uint8_t c) = 0;
  virtual void flush() { }

  virtual size_t write(const uint8_t* buf, size_t len) {
    size_t n = 0;
    for (size_t i = 0; i < len; i++) n += write(buf[i]);
    return n;
  }

  size_t print(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(int v) { return printNumber((long)v); }
  size_t print(long v) { return printNumber(v); }
  size_t print(unsigned long v) { return printUNumber(v); }
  size_t print(uint32_t v) { return printUNumber(v); }

  size_t println() { return write((uint8_t)'\n'); }
  size_t println(const char* s) { return print(s) + println(); }

  size_t printf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n <= 0) return 0;
    return write((const uint8_t*)buf, (size_t)(n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1));
  }

private:
  size_t printNumber(long v) {
    char buf[24];
    int n = snprintf(buf, sizeof(buf), "%ld", v);
    return write((const uint8_t*)buf, (size_t)n);
  }
  size_t printUNumber(unsigned long v) {
    char buf[24];
    int n = snprintf(buf, sizeof(buf), "%lu", v);
    return write((const uint8_t*)buf, (size_t)n);
  }
};

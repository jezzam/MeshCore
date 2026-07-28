#pragma once

// Minimal adafruit/RTClib shim for LINUX_PLATFORM - just the DateTime class
// (pure epoch/calendar math, no RTC-chip I2C driver), matching the exact
// usage in examples/simple_repeater/MyMesh.cpp's getLogDateTime(): construct
// from a UNIX epoch uint32_t, read back hour/minute/second/day/month/year.

#include <cstdint>
#include <ctime>

class DateTime {
public:
  DateTime() : DateTime((uint32_t)0) { }
  explicit DateTime(uint32_t epoch) {
    time_t t = (time_t)epoch;
    gmtime_r(&t, &_tm);
  }

  int hour() const { return _tm.tm_hour; }
  int minute() const { return _tm.tm_min; }
  int second() const { return _tm.tm_sec; }
  int day() const { return _tm.tm_mday; }
  int month() const { return _tm.tm_mon + 1; }
  int year() const { return _tm.tm_year + 1900; }

private:
  struct tm _tm;
};

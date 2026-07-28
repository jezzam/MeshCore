#pragma once

// Minimal but real (not a no-op stub) CayenneLPP encoder for LINUX_PLATFORM -
// scope matches actual usage in examples/simple_repeater/MyMesh.cpp (self
// telemetry: voltage + temperature) and src/helpers/SensorManager.h's
// querySensors() signature. Real encoding (not just compile-compatible)
// since getBuffer()/getSize() output is memcpy'd straight into an on-air
// reply packet - a no-op stub would silently send garbage/uninitialized
// bytes to real MeshCore clients querying this repeater's telemetry.
// Standard Cayenne LPP data type IDs: analog voltage = 2 (0.01V signed
// 16-bit), temperature = 103/0x67 (0.1C signed 16-bit) - matches the
// electroniccats/CayenneLPP library's encoding for these two types.

#include <cstdint>
#include <cstring>

class CayenneLPP {
public:
  explicit CayenneLPP(uint8_t maxsize = 64) : _maxsize(maxsize), _cursor(0) {
    _buffer = new uint8_t[maxsize];
  }
  ~CayenneLPP() { delete[] _buffer; }

  void reset() { _cursor = 0; }
  uint8_t getSize() const { return _cursor; }
  uint8_t* getBuffer() const { return _buffer; }

  uint8_t addVoltage(uint8_t channel, float volts) {
    return addSigned16(channel, 2, (int32_t)(volts * 100));
  }

  uint8_t addTemperature(uint8_t channel, float celsius) {
    return addSigned16(channel, 0x67, (int32_t)(celsius * 10));
  }

private:
  uint8_t addSigned16(uint8_t channel, uint8_t type, int32_t scaled) {
    if (_cursor + 4 > _maxsize) return 0;
    _buffer[_cursor++] = channel;
    _buffer[_cursor++] = type;
    _buffer[_cursor++] = (uint8_t)(scaled >> 8);
    _buffer[_cursor++] = (uint8_t)(scaled & 0xFF);
    return _cursor;
  }

  uint8_t* _buffer;
  uint8_t _maxsize;
  uint8_t _cursor;
};

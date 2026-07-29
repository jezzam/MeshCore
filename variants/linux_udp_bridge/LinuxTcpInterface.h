#pragma once

// TCP companion-protocol transport for the phone app (LAN, no BLE needed) -
// a Linux/POSIX-socket port of src/helpers/esp32/SerialWifiInterface.h's
// frame protocol (same 3-byte header, same single-active-client model), so
// the official MeshCore app just needs this device's IP:port to connect.

#include <helpers/BaseSerialInterface.h>

class LinuxTcpInterface : public BaseSerialInterface {
  bool _deviceConnected;
  bool _isEnabled;

  int _listen_fd;
  int _client_fd;

  struct FrameHeader {
    uint8_t type;
    uint16_t length;
  };

  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];
  };

  // Partial-header/partial-payload read state, since recv() on a
  // non-blocking socket can return any number of bytes at a time - unlike
  // ESP32's WiFiClient (backed by lwIP's own buffered stream), a raw POSIX
  // socket gives no "wait until N bytes available" primitive, so incomplete
  // reads have to be accumulated across calls instead of assumed complete.
  uint8_t _header_buf[3];
  int _header_have;
  FrameHeader _recv_header;
  uint8_t _payload_buf[MAX_FRAME_SIZE];
  int _payload_have;

  #define TCP_SEND_QUEUE_SIZE  4
  int _send_queue_len;
  Frame _send_queue[TCP_SEND_QUEUE_SIZE];

  bool hasReceivedFrameHeader() const { return _recv_header.type != 0 && _recv_header.length != 0; }
  void resetReceivedFrameHeader() { _recv_header.type = 0; _recv_header.length = 0; _header_have = 0; _payload_have = 0; }
  void acceptPendingConnection();
  void closeClient();

public:
  LinuxTcpInterface();

  void begin(int port);

  // BaseSerialInterface methods
  void enable() override;
  void disable() override;
  bool isEnabled() const override { return _isEnabled; }

  bool isConnected() const override;
  bool isWriteBusy() const override;

  size_t writeFrame(const uint8_t src[], size_t len) override;
  size_t checkRecvFrame(uint8_t dest[]) override;
};

#if TCP_DEBUG_LOGGING && ARDUINO
  #include <Arduino.h>
  #define TCP_DEBUG_PRINT(F, ...) Serial.printf("TCP: " F, ##__VA_ARGS__)
  #define TCP_DEBUG_PRINTLN(F, ...) Serial.printf("TCP: " F "\n", ##__VA_ARGS__)
#else
  #define TCP_DEBUG_PRINT(...) {}
  #define TCP_DEBUG_PRINTLN(...) {}
#endif

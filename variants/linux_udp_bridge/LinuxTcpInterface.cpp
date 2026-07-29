#include "LinuxTcpInterface.h"
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

static void setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

LinuxTcpInterface::LinuxTcpInterface()
  : _deviceConnected(false), _isEnabled(false), _listen_fd(-1), _client_fd(-1),
    _header_have(0), _payload_have(0), _send_queue_len(0) {
  _recv_header.type = 0;
  _recv_header.length = 0;
}

void LinuxTcpInterface::begin(int port) {
  _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (_listen_fd < 0) {
    TCP_DEBUG_PRINTLN("socket() failed");
    return;
  }
  int one = 1;
  setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;   // reachable on the LAN via network_mode: host
  addr.sin_port = htons((uint16_t)port);

  if (bind(_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    TCP_DEBUG_PRINTLN("bind() failed, port=%d", port);
    ::close(_listen_fd);
    _listen_fd = -1;
    return;
  }
  listen(_listen_fd, 1);   // single-client model, matching SerialWifiInterface
  setNonBlocking(_listen_fd);
}

void LinuxTcpInterface::enable() {
  if (_isEnabled) return;
  _isEnabled = true;
  _send_queue_len = 0;
}

void LinuxTcpInterface::disable() {
  _isEnabled = false;
}

void LinuxTcpInterface::closeClient() {
  if (_client_fd >= 0) {
    ::close(_client_fd);
    _client_fd = -1;
  }
  _deviceConnected = false;
  resetReceivedFrameHeader();
  _send_queue_len = 0;
}

void LinuxTcpInterface::acceptPendingConnection() {
  if (_listen_fd < 0) return;
  int newfd = accept(_listen_fd, nullptr, nullptr);
  if (newfd < 0) return;   // EAGAIN/EWOULDBLOCK: nothing pending

  // disconnect existing client, switch to the new one (matches
  // SerialWifiInterface::checkRecvFrame()'s single-active-connection model)
  closeClient();

  int one = 1;
  setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  setNonBlocking(newfd);

  _client_fd = newfd;
  _deviceConnected = true;
  TCP_DEBUG_PRINTLN("Got connection");
}

bool LinuxTcpInterface::isConnected() const { return _deviceConnected; }
bool LinuxTcpInterface::isWriteBusy() const { return false; }

size_t LinuxTcpInterface::writeFrame(const uint8_t src[], size_t len) {
  if (len > MAX_FRAME_SIZE) {
    TCP_DEBUG_PRINTLN("writeFrame(), frame too big, len=%d", (int)len);
    return 0;
  }
  if (_deviceConnected && len > 0) {
    if (_send_queue_len >= TCP_SEND_QUEUE_SIZE) {
      TCP_DEBUG_PRINTLN("writeFrame(), send_queue is full!");
      return 0;
    }
    _send_queue[_send_queue_len].len = (uint8_t)len;
    memcpy(_send_queue[_send_queue_len].buf, src, len);
    _send_queue_len++;
    return len;
  }
  return 0;
}

size_t LinuxTcpInterface::checkRecvFrame(uint8_t dest[]) {
  acceptPendingConnection();

  if (!_deviceConnected) return 0;

  // send queue takes priority, same as SerialWifiInterface
  if (_send_queue_len > 0) {
    int len = _send_queue[0].len;
    uint8_t pkt[3 + MAX_FRAME_SIZE];
    pkt[0] = '>';                          // device -> app, matching the wire protocol
    pkt[1] = (uint8_t)(len & 0xFF);        // length LSB
    pkt[2] = (uint8_t)(len >> 8);          // length MSB
    memcpy(&pkt[3], _send_queue[0].buf, len);

    ssize_t sent = send(_client_fd, pkt, 3 + len, MSG_NOSIGNAL);
    if (sent < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        TCP_DEBUG_PRINTLN("send() failed, disconnecting");
        closeClient();
      }
      return 0;
    }
    // Best-effort: MAX_FRAME_SIZE-sized frames practically never partial-write
    // on a local/LAN socket - treat any successful send() as fully delivered
    // rather than adding partial-write retry bookkeeping.
    _send_queue_len--;
    for (int i = 0; i < _send_queue_len; i++) _send_queue[i] = _send_queue[i + 1];
    return 0;
  }

  // accumulate the 3-byte frame header across calls - unlike ESP32's
  // WiFiClient (buffered by lwIP), a raw non-blocking recv() can return a
  // partial header, so completion has to be tracked across invocations
  if (!hasReceivedFrameHeader()) {
    while (_header_have < 3) {
      ssize_t n = recv(_client_fd, _header_buf + _header_have, 3 - _header_have, 0);
      if (n > 0) {
        _header_have += (int)n;
      } else if (n == 0) {
        TCP_DEBUG_PRINTLN("Disconnected");
        closeClient();
        return 0;
      } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) closeClient();
        return 0;   // no more data available right now, try again next call
      }
    }
    _recv_header.type = _header_buf[0];
    _recv_header.length = (uint16_t)_header_buf[1] | ((uint16_t)_header_buf[2] << 8);
  }

  if (!hasReceivedFrameHeader()) return 0;   // e.g. a zero-length header, wait for a real one

  int frame_length = _recv_header.length;
  int frame_type = _recv_header.type;

  if (frame_length > MAX_FRAME_SIZE) {
    // drain and discard via a scratch buffer - never write past _payload_buf
    uint8_t scratch[64];
    while (_payload_have < frame_length) {
      int want = frame_length - _payload_have;
      if (want > (int)sizeof(scratch)) want = sizeof(scratch);
      ssize_t n = recv(_client_fd, scratch, want, 0);
      if (n > 0) {
        _payload_have += (int)n;
      } else if (n == 0) {
        closeClient();
        return 0;
      } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) closeClient();
        return 0;
      }
    }
    TCP_DEBUG_PRINTLN("Skipped oversized frame: length=%d", frame_length);
    resetReceivedFrameHeader();
    return 0;
  }

  while (_payload_have < frame_length) {
    ssize_t n = recv(_client_fd, _payload_buf + _payload_have, frame_length - _payload_have, 0);
    if (n > 0) {
      _payload_have += (int)n;
    } else if (n == 0) {
      TCP_DEBUG_PRINTLN("Disconnected");
      closeClient();
      return 0;
    } else {
      if (errno != EAGAIN && errno != EWOULDBLOCK) closeClient();
      return 0;   // wait for more data next call
    }
  }

  // full frame received
  if (frame_type != '<') {   // '<' = frame sent from app to radio, per the wire protocol
    TCP_DEBUG_PRINTLN("Skipping frame: type=0x%x is unexpected", frame_type);
    resetReceivedFrameHeader();
    return 0;
  }

  memcpy(dest, _payload_buf, frame_length);
  resetReceivedFrameHeader();
  return frame_length;
}

#include "PktFwdRadio.h"
#include <Arduino.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sodium.h>
#include <sys/socket.h>
#include <unistd.h>

#include "compat/lib/nlohmann/json.hpp"
using json = nlohmann::json;

// Semtech packet-forwarder protocol message identifiers (PROTOCOL.TXT, Lora-net/packet_forwarder).
#define PKT_PUSH_DATA 0x00
#define PKT_PUSH_ACK  0x01
#define PKT_PULL_DATA 0x02
#define PKT_PULL_ACK  0x04
#define PKT_PULL_RESP 0x03
#define PKT_TX_ACK    0x05

#ifndef LORA_FREQ
#define LORA_FREQ 904.6
#endif
#ifndef LORA_BW
#define LORA_BW 500
#endif
#ifndef LORA_SF
#define LORA_SF 8
#endif
#ifndef LORA_CR
#define LORA_CR 5
#endif

void PktFwdRadio::begin() {
  // Dispatcher::begin() (src/Dispatcher.cpp) calls _radio->begin() as part of
  // its normal startup sequence, in addition to our own explicit call from
  // radio_init() (target.cpp) - harmless for real radio hardware (RadioLibWrapper::
  // begin() just re-arms interrupts/state), but our socket creation isn't
  // naturally idempotent, so guard explicitly. Found via a real bug: a naive
  // unconditional re-create silently produced a second, never-bound socket
  // (the bind failed with the port already in use by the first socket, but
  // MESH_DEBUG_PRINTLN is a no-op without -DMESH_DEBUG, so the failure was
  // completely silent) - loop() then span forever reading from the wrong,
  // unbound fd, invisibly receiving nothing.
  if (_sock >= 0) return;

  if (sodium_init() < 0) {
    Serial.println("PktFwdRadio: sodium_init() failed");
  }

  _sock = socket(AF_INET, SOCK_DGRAM, 0);
  int flags = fcntl(_sock, F_GETFL, 0);
  fcntl(_sock, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(1680);

  if (bind(_sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    Serial.printf("PktFwdRadio: bind(127.0.0.1:1680) failed: %s\n", strerror(errno));
    close(_sock);
    _sock = -1;
  } else {
    Serial.println("PktFwdRadio: bound 127.0.0.1:1680, waiting for lora_pkt_fwd_1302...");
  }
}

void PktFwdRadio::loop() {
  if (_sock < 0) return;

  uint8_t buf[2048];
  struct sockaddr_in from;
  socklen_t fromlen = sizeof(from);

  for (;;) {  // drain all pending datagrams this cycle, non-blocking
    ssize_t n = recvfrom(_sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
    if (n <= 0) break;
    handleDatagram(buf, (int)n, from);
  }
}

void PktFwdRadio::handleDatagram(const uint8_t* buf, int len, const struct sockaddr_in& from) {
  if (len < 4 || buf[0] != 2) return;  // protocol version 2 only
  uint8_t id = buf[3];
  if (id == PKT_PUSH_DATA) handlePushData(buf, len, from);
  else if (id == PKT_PULL_DATA) handlePullData(buf, len, from);
  // TX_ACK (0x05) intentionally not parsed for errors yet - see Research_beginning_notes.md Phase 3 notes.
}

void PktFwdRadio::sendAck(uint8_t ack_id, const uint8_t* orig_hdr, const struct sockaddr_in& to) {
  uint8_t ack[4] = {2, orig_hdr[1], orig_hdr[2], ack_id};
  sendto(_sock, ack, sizeof(ack), 0, (const struct sockaddr*)&to, sizeof(to));
}

void PktFwdRadio::handlePushData(const uint8_t* buf, int len, const struct sockaddr_in& from) {
  sendAck(PKT_PUSH_ACK, buf, from);  // spec requires acking immediately, before further processing

  if (len <= 12) return;  // no JSON payload (shouldn't happen for PUSH_DATA, but be defensive)
  json j = json::parse(buf + 12, buf + len, nullptr, false);
  if (j.is_discarded() || !j.contains("rxpk")) return;

  for (auto& pk : j["rxpk"]) {
    if (_rx_count >= RX_QUEUE_SIZE) {
      _n_recv_errors++;
      break;  // queue full - MyMesh isn't draining fast enough, drop rather than block
    }
    if (!pk.contains("data")) continue;
    std::string b64 = pk["data"].get<std::string>();

    RxEntry& e = _rx_queue[_rx_tail];
    size_t decoded_len = 0;
    int rc = sodium_base642bin(e.data, sizeof(e.data), b64.c_str(), b64.size(),
                                nullptr, &decoded_len, nullptr, sodium_base64_VARIANT_ORIGINAL);
    if (rc != 0 || decoded_len == 0) { _n_recv_errors++; continue; }

    e.len = (int)decoded_len;
    e.rssi = pk.value("rssi", 0.0f);
    e.snr = pk.value("lsnr", 0.0f);

    _rx_tail = (_rx_tail + 1) % RX_QUEUE_SIZE;
    _rx_count++;
  }
}

void PktFwdRadio::handlePullData(const uint8_t* buf, int len, const struct sockaddr_in& from) {
  sendAck(PKT_PULL_ACK, buf, from);

  // The gateway's current UDP source address/port - the only way we learn
  // where to send PULL_RESP downlink packets (it may be behind NAT in
  // general; here it's always localhost, but we follow the protocol
  // correctly regardless rather than hardcoding the assumption).
  _downlink_addr = from;
  _have_downlink_addr = true;
  _last_pull_data_at = millis();
}

int PktFwdRadio::recvRaw(uint8_t* bytes, int sz) {
  if (_rx_count == 0) return 0;

  RxEntry& e = _rx_queue[_rx_head];
  int n = e.len < sz ? e.len : sz;
  memcpy(bytes, e.data, n);
  _last_rssi = e.rssi;
  _last_snr = e.snr;

  _rx_head = (_rx_head + 1) % RX_QUEUE_SIZE;
  _rx_count--;
  _n_recv++;
  return n;
}

bool PktFwdRadio::startSendRaw(const uint8_t* bytes, int len) {
  if (!_have_downlink_addr) {
    MESH_DEBUG_PRINTLN("PktFwdRadio: cannot send, no PULL_DATA seen yet from lora_pkt_fwd_1302");
    return false;
  }

  char b64[512];
  size_t b64_len = 0;
  sodium_bin2base64(b64, sizeof(b64), bytes, (size_t)len, sodium_base64_VARIANT_ORIGINAL);
  b64_len = strlen(b64);

  char datr[16];
  snprintf(datr, sizeof(datr), "SF%dBW%d", LORA_SF, LORA_BW);

  json txpk = {
    {"imme", true},  // Dispatcher already owns TX timing - no need for tmst-synchronized send
    {"freq", LORA_FREQ},
    {"rfch", 0},
    {"powe", (int)_tx_power_dbm},
    {"modu", "LORA"},
    {"datr", datr},
    {"codr", std::string("4/") + std::to_string(LORA_CR)},
    {"size", len},
    {"data", std::string(b64, b64_len)},
  };
  json root = {{"txpk", txpk}};
  std::string payload = root.dump();

  uint16_t token = (uint16_t)(randombytes_random() & 0xFFFF);
  uint8_t hdr[4] = {2, (uint8_t)(token >> 8), (uint8_t)(token & 0xFF), PKT_PULL_RESP};

  uint8_t pkt[4 + 1024];
  if (payload.size() > sizeof(pkt) - 4) return false;
  memcpy(pkt, hdr, 4);
  memcpy(pkt + 4, payload.data(), payload.size());

  ssize_t sent = sendto(_sock, pkt, 4 + payload.size(), 0,
                         (const struct sockaddr*)&_downlink_addr, sizeof(_downlink_addr));
  if (sent <= 0) return false;

  _send_deadline = millis() + getEstAirtimeFor(len);
  _sending = true;
  _n_sent++;
  return true;
}

bool PktFwdRadio::isSendComplete() {
  // No reliable "physically finished transmitting" signal from the packet-
  // forwarder protocol (TX_ACK is only a best-effort accept/reject notice,
  // not a completion event) - estimate via airtime instead, matching how a
  // real radio's "done" flag/IRQ would be polled.
  if (!_sending) return true;
  if ((long)(millis() - _send_deadline) >= 0) { _sending = false; return true; }
  return false;
}

uint32_t PktFwdRadio::getEstAirtimeFor(int len_bytes) {
  // Standard Semtech LoRa airtime formula (AN1200.13) against our fixed
  // SF/BW/CR. Preamble uses the LoRa default of 8 symbols - unlike
  // RadioLibWrapper's boosted 32/16-symbol convention (src/helpers/radiolib/
  // RadioLibWrappers.h), which is a real-hardware-specific reliability
  // choice we don't control here: the concentrator's actual preamble is
  // configured independently in global_conf_1302.json (not exposed in the
  // rxpk/txpk JSON, "prea" is optional and we don't set it), so this is an
  // estimate for MeshCore's own scheduling, not a transmitted parameter.
  const double bw_hz = (double)LORA_BW * 1000.0;
  const int sf = LORA_SF;
  const int cr = LORA_CR - 4;  // "4/5".."4/8" -> 1..4
  const int n_preamble = 8;
  const int H = 0;   // explicit header
  const int CRC = 1; // payload CRC enabled

  double t_sym_ms = (double)(1u << sf) / bw_hz * 1000.0;
  int DE = (t_sym_ms > 16.0) ? 1 : 0;
  double t_preamble_ms = (n_preamble + 4.25) * t_sym_ms;

  int numerator = 8 * len_bytes - 4 * sf + 28 + 16 * CRC - 20 * H;
  int denom = 4 * (sf - 2 * DE);
  int n_payload_symbols = 8;
  if (numerator > 0 && denom > 0) {
    int payload_symbol_count = (numerator + denom - 1) / denom;  // ceil
    n_payload_symbols += payload_symbol_count * (cr + 4);
  }
  double t_payload_ms = n_payload_symbols * t_sym_ms;

  return (uint32_t)(t_preamble_ms + t_payload_ms + 0.5);
}

// Ported verbatim from src/helpers/radiolib/RadioLibWrappers.cpp's
// packetScoreInt, using our actual fixed SF (upstream hardcodes sf=10 as a
// default since real hardware can dynamically receive at multiple SFs -
// we always operate at LORA_SF, so use the real value for accuracy).
static float snr_threshold[] = {-7.5, -10, -12.5, -15, -17.5, -20};  // SF7..SF12

float PktFwdRadio::packetScore(float snr, int packet_len) {
  const int sf = LORA_SF;
  if (sf < 7 || sf > 12) return 0.0f;
  if (snr < snr_threshold[sf - 7]) return 0.0f;

  float success_rate = (snr - snr_threshold[sf - 7]) / 10.0f;
  float collision_penalty = 1.0f - (packet_len / 256.0f);
  float score = success_rate * collision_penalty;
  return score < 0.0f ? 0.0f : (score > 1.0f ? 1.0f : score);
}

uint32_t PktFwdRadio::getRngSeed() {
  return randombytes_random();
}

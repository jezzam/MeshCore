#pragma once

#include <Dispatcher.h>
#include <cstdint>
#include <cstdlib>
#include <netinet/in.h>

// Bridges MeshCore's mesh::Radio interface to the Semtech UDP packet-forwarder
// protocol already spoken by the host's lora_pkt_fwd_1302 process (see
// Research_beginning_notes.md for the full protocol reference and interface
// mapping). We act as the "server" role - lora_pkt_fwd_1302 is already
// configured to talk to 127.0.0.1:1680, which is exactly what we bind.
//
// No SPI/hardware access at all: this class is a pure UDP client/server. All
// actual radio work is done by lora_pkt_fwd_1302, left running exactly as-is.
class PktFwdRadio : public mesh::Radio {
public:
  void begin() override;

  int recvRaw(uint8_t* bytes, int sz) override;
  uint32_t getEstAirtimeFor(int len_bytes) override;
  float packetScore(float snr, int packet_len) override;
  bool startSendRaw(const uint8_t* bytes, int len) override;
  bool isSendComplete() override;
  void onSendFinished() override { }
  void loop() override;
  bool isInRecvMode() const override { return true; }
  float getLastRSSI() const override { return _last_rssi; }
  float getLastSNR() const override { return _last_snr; }

  // RadioLibWrapper-parity surface (called directly by MyMesh.cpp):
  uint32_t getRngSeed();
  void setTxPower(int8_t dbm) { _tx_power_dbm = dbm; }
  void setParams(float, float, uint8_t, uint8_t) { /* channel plan is fixed in global_conf_1302.json, not retuned here */ }
  uint32_t getPacketsRecv() const { return _n_recv; }
  uint32_t getPacketsSent() const { return _n_sent; }
  uint32_t getPacketsRecvErrors() const { return _n_recv_errors; }
  void resetStats() { _n_recv = _n_sent = _n_recv_errors = 0; }
  void setRxBoostedGainMode(bool) { }
  bool getRxBoostedGainMode() const { return false; }

private:
  static constexpr int RX_QUEUE_SIZE = 8;
  static constexpr int MAX_PKT_LEN = 256;

  struct RxEntry {
    uint8_t data[MAX_PKT_LEN];
    int len;
    float rssi, snr;
  };

  void handleDatagram(const uint8_t* buf, int len, const struct sockaddr_in& from);
  void handlePushData(const uint8_t* buf, int len, const struct sockaddr_in& from);
  void handlePullData(const uint8_t* buf, int len, const struct sockaddr_in& from);
  void sendAck(uint8_t ack_id, const uint8_t* orig_hdr, const struct sockaddr_in& to);

  int _sock = -1;
  bool _have_downlink_addr = false;
  struct sockaddr_in _downlink_addr;
  unsigned long _last_pull_data_at = 0;

  RxEntry _rx_queue[RX_QUEUE_SIZE];
  int _rx_head = 0, _rx_tail = 0, _rx_count = 0;

  float _last_rssi = 0, _last_snr = 0;
  int8_t _tx_power_dbm = 20;
  unsigned long _send_deadline = 0;
  bool _sending = false;

  uint32_t _n_recv = 0, _n_sent = 0, _n_recv_errors = 0;
};

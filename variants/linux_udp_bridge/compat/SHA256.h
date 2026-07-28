#pragma once

// Real (not mocked) SHA256/HMAC-SHA256 shim for LINUX_PLATFORM, matching the
// rweather/Crypto SHA256 API surface that src/Utils.cpp calls (update/finalize/
// resetHMAC/finalizeHMAC - verified against test/mocks/SHA256.h's method
// signatures, which exist for unit tests only and are no-ops there).
// Must be byte-for-byte standards-compliant: this repeater has to interoperate
// with real MeshCore devices over the air, not just be internally self-consistent.
// Implemented on libsodium's crypto_hash_sha256 / crypto_auth_hmacsha256 primitives.

#include <sodium.h>
#include <cstdint>
#include <cstring>

class SHA256 {
public:
  SHA256() : _hmac(false) { crypto_hash_sha256_init(&_state); }

  // const void* (not const uint8_t*): real rweather/Crypto accepts any
  // pointer type here - callers across this codebase pass uint16_t*, char*,
  // and uint8_t* interchangeably (e.g. src/Packet.cpp hashes a raw uint16_t
  // field directly).
  void update(const void* data, size_t len) {
    if (_hmac) crypto_auth_hmacsha256_update(&_hmac_state, (const unsigned char*)data, len);
    else crypto_hash_sha256_update(&_state, (const unsigned char*)data, len);
  }

  // Truncates to hashLen bytes, matching mesh::Utils::sha256's contract.
  void finalize(void* hash, size_t hashLen) {
    uint8_t full[crypto_hash_sha256_BYTES];
    crypto_hash_sha256_final(&_state, full);
    memcpy(hash, full, hashLen < sizeof(full) ? hashLen : sizeof(full));
  }

  void resetHMAC(const void* key, size_t keyLen) {
    _hmac = true;
    crypto_auth_hmacsha256_init(&_hmac_state, (const unsigned char*)key, keyLen);
  }

  void finalizeHMAC(const void*, size_t, void* hash, size_t hashLen) {
    uint8_t full[crypto_auth_hmacsha256_BYTES];
    crypto_auth_hmacsha256_final(&_hmac_state, full);
    memcpy(hash, full, hashLen < sizeof(full) ? hashLen : sizeof(full));
  }

private:
  bool _hmac;
  crypto_hash_sha256_state _state;
  crypto_auth_hmacsha256_state _hmac_state;
};

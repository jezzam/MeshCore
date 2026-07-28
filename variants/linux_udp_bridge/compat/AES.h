#pragma once

// Real (not mocked) AES-128-ECB shim for LINUX_PLATFORM, matching the
// rweather/Crypto AES128 API surface that src/Utils.cpp calls (setKey/
// encryptBlock/decryptBlock - verified against test/mocks/AES.h and the
// actual per-block-ECB-loop usage in Utils::encrypt/decrypt, no IV/mode
// beyond plain ECB is used anywhere in this codebase).
// Implemented on vendored tiny-AES-c (lib/tiny-aes-c/), a small public-domain
// single-file AES implementation - chosen over libsodium because libsodium
// deliberately does not expose a raw unauthenticated AES-ECB primitive.

extern "C" {
#include "lib/tiny-aes-c/aes.h"
}
// tiny-AES-c unconditionally #define's AES128 as a build-config flag (its
// key-size selector) - undefine it before declaring our class of the same
// name, or "class AES128 {" expands to "class 1 {".
#undef AES128
#include <cstdint>
#include <cstring>

class AES128 {
public:
  void setKey(const uint8_t* key, size_t) {
    AES_init_ctx(&_ctx, key);
  }

  // tiny-AES-c's AES_ECB_encrypt/decrypt operate in-place on a single 16-byte block.
  void encryptBlock(uint8_t* output, const uint8_t* input) {
    memcpy(output, input, 16);
    AES_ECB_encrypt(&_ctx, output);
  }

  void decryptBlock(uint8_t* output, const uint8_t* input) {
    memcpy(output, input, 16);
    AES_ECB_decrypt(&_ctx, output);
  }

private:
  struct AES_ctx _ctx;
};

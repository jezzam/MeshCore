#pragma once

// Thin compat shim for densaugeo/base64 (the PlatformIO lib real boards use)
// - only the one function BaseChatMesh.cpp actually calls (decode_base64(),
// for parsing a group-channel's PSK). Wraps libsodium's own base64 codec,
// already a build dependency here (see PktFwdRadio.cpp for the same
// sodium_base642bin() pattern used for LoRa payload encode/decode) - no
// need to vendor a whole separate library for one function.

#include <sodium.h>

// densaugeo/base64's real signature takes no output-buffer-size parameter -
// callers are trusted to have sized `output` correctly (BaseChatMesh.cpp's
// one call site decodes into a fixed uint8_t secret[PUB_KEY_SIZE] = 32-byte
// buffer). Passing inputLen as libsodium's bin_maxlen would be unsafe -
// decoded length is bounded by inputLen but the *real* destination isn't -
// so cap at 32 (PUB_KEY_SIZE) instead of trusting an attacker/caller-
// controlled inputLen, matching the only real destination size this is ever
// called with.
inline unsigned int decode_base64(const unsigned char* input, unsigned int inputLen, unsigned char* output) {
  size_t decoded_len = 0;
  int rc = sodium_base642bin(output, 32, (const char*)input, inputLen,
                              nullptr, &decoded_len, nullptr, sodium_base64_VARIANT_ORIGINAL);
  return rc == 0 ? (unsigned int)decoded_len : 0;
}

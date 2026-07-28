#pragma once

// Shim for the rweather/Crypto Ed25519 class, used by src/Identity.cpp for
// ONLY signature verification (Identity::verify) - key generation/ECDH still
// go through MeshCore's own vendored lib/ed25519/ C code (ed25519_create_keypair,
// ed25519_key_exchange, etc.), which is already plain portable C with no
// Arduino dependency, so keys stay bit-compatible with every other MeshCore
// device. The raw ed25519_verify() from that vendored lib is deliberately
// disabled upstream (see src/Identity.cpp, "memory corruption bug" comment) -
// libsodium's well-audited verify is used here instead, standard RFC 8032
// Ed25519, cross-compatible with any conformant signer.

#include <sodium.h>
#include <cstdint>

class Ed25519 {
public:
  static bool verify(const uint8_t* signature, const uint8_t* pubkey, const uint8_t* message, size_t msg_len) {
    return crypto_sign_ed25519_verify_detached(signature, message, msg_len, pubkey) == 0;
  }
};

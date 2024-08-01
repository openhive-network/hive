#pragma once
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/sha512.hpp>
#include <fc/crypto/hmac.hpp>


namespace fc {
  // this file implements the entropy derivation defined in bip85
  // it doesn't include the deterministic random number generator, since we have
  // no need for it and don't have a sha3 implementation handy

  // get a full 512 bits of entropy
  inline sha512 compute_bip85_entropy_from_key(const ecc::private_key& k) {
    const sha256 secret = k.get_secret();
    return hmac<sha512>().digest("bip-entropy-from-k", strlen("bip-entropy-from-k"),
                                 (const char*)&secret, sizeof(secret));
  }

  // helper to get only the first n bytes of entropy (so for 128 bits, pass 16)
  inline std::vector<unsigned char> compute_bip85_entropy_from_key(const ecc::private_key& k, size_t bytes_required) {
    FC_ASSERT(bytes_required <= 64);
    const sha512 full_entropy = compute_bip85_entropy_from_key(k);
    return std::vector<unsigned char>((const unsigned char*)&full_entropy, (const unsigned char*)&full_entropy + bytes_required);
  }
} 

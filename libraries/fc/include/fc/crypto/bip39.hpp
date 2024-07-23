#pragma once
#include <fc/crypto/elliptic.hpp>
#include <vector>
#include <bitset>

namespace fc {
  template<typename HMAC>
  std::vector<unsigned char> pbkdf2(std::string_view password, std::string_view salt, uint32_t iterations, uint32_t dk_len)
  {
    const uint32_t h_len = typename HMAC::hash_type().data_size();
    const uint32_t l = (dk_len + h_len - 1) / h_len;
    const uint32_t r = dk_len - (l - 1) * h_len;

    std::vector<unsigned char> output(dk_len);
    std::vector<unsigned char> u(h_len);
    std::vector<unsigned char> t(h_len);
    std::vector<unsigned char> block(salt.size() + 4);

    std::memcpy(block.data(), salt.data(), salt.size());

    for (uint32_t i = 1; i <= l; i++)
    {
      block[salt.size()] = static_cast<unsigned char>((i >> 24) & 0xff);
      block[salt.size() + 1] = static_cast<unsigned char>((i >> 16) & 0xff);
      block[salt.size() + 2] = static_cast<unsigned char>((i >> 8) & 0xff);
      block[salt.size() + 3] = static_cast<unsigned char>(i & 0xff);

      auto hmac_result = HMAC().digest(password.data(), password.size(), reinterpret_cast<const char*>(block.data()), block.size());
      std::memcpy(u.data(), hmac_result.data(), h_len);

      std::copy(u.begin(), u.end(), t.begin());

      for (uint32_t j = 1; j < iterations; j++)
      {
        hmac_result = HMAC().digest(password.data(), password.size(), reinterpret_cast<const char*>(u.data()), h_len);
        std::memcpy(u.data(), hmac_result.data(), h_len);
        for (uint32_t k = 0; k < h_len; k++)
        {
          t[k] ^= u[k];
        }
      }

      std::memcpy(output.data() + (i - 1) * h_len, t.data(), (i == l) ? r : h_len);
    }

    return output;
  }

  std::string generate_bip39_mnemonic(const std::vector<unsigned char>& entropy);
  // converts a mnemonic to an extended private key
  // note: this does not normalize the string.  The caller must collapse extraneous spaces, convert the string to 
  // UTF-8 NFKD format
  fc::ecc::extended_private_key bip39_mnemonic_to_extended_private_key(std::string_view mnemonic, std::string_view passphrase = "");
} 

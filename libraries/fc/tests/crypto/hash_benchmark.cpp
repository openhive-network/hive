
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/cripemd160.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/sha512.hpp>
#include <fc/crypto/rand.hpp>
#include <fc/exception/exception.hpp>
#include <openssl/ripemd.h>
#include <openssl/sha.h>

#include <iostream>


void test_openssl(const char *buf, size_t size) {
  fc::time_point openssl_begin = fc::time_point::now();
  for (unsigned i = 0; i < 10000; ++i)
  {
    unsigned char hash[20];
    RIPEMD160((const unsigned char*)buf, size, hash);
  }
  fc::time_point openssl_end = fc::time_point::now();
  fc::microseconds openssl_duration = openssl_end - openssl_begin;
  uint32_t each = openssl_duration.count() / 10000;
  ilog("OpenSSL RIPEMD160 raw calls, 64k bytes, 10,000 iterations: ${openssl_duration}μs, ${each}μs per", (openssl_duration)(each));
}

void test_fc(const char *buf, size_t size) {
  fc::time_point fc_begin = fc::time_point::now();
  for (unsigned i = 0; i < 10000; ++i)
    fc::ripemd160::hash(buf, size);
  fc::time_point fc_end = fc::time_point::now();
  fc::microseconds fc_duration = fc_end - fc_begin;
  uint32_t each = fc_duration.count() / 10000;
  ilog("fc::ripemd160::hash calls, 64k bytes, 10,000 iterations: ${fc_duration}μs, ${each}μs per", (fc_duration)(each));
}

void test_btc(const char *buf, size_t size) {
  fc::time_point btc_begin = fc::time_point::now();
  for (unsigned i = 0; i < 10000; ++i)
  {
    unsigned char hash[20];
    CRIPEMD160 hasher;
    hasher.Write((const unsigned char*)buf, size);
    hasher.Finalize(hash);
  }
  fc::time_point btc_end = fc::time_point::now();
  fc::microseconds btc_duration = btc_end - btc_begin;
  uint32_t each = btc_duration.count() / 10000;
  ilog("BTC CRIPEMD160 calls, 64k bytes, 10,000 iterations: ${btc_duration}μs, ${each}μs per", (btc_duration)(each));
}

void test_btc_reuse(const char *buf, size_t size) {
  fc::time_point btc_begin = fc::time_point::now();
  CRIPEMD160 hasher;
  for (unsigned i = 0; i < 10000; ++i)
  {
    unsigned char hash[20];
    hasher.Write((const unsigned char*)buf, size);
    hasher.Finalize(hash);
    hasher.Reset();
  }
  fc::time_point btc_end = fc::time_point::now();
  fc::microseconds btc_duration = btc_end - btc_begin;
  uint32_t each = btc_duration.count() / 10000;
  ilog("BTC CRIPEMD160 calls, reused, 64k bytes, 10,000 iterations: ${btc_duration}μs, ${each}μs per", (btc_duration)(each));
}

void test_openssl_sha256(const char *buf, size_t size) {
  fc::time_point openssl_begin = fc::time_point::now();
  for (unsigned i = 0; i < 10000; ++i)
  {
    unsigned char hash[32];
    SHA256((const unsigned char*)buf, size, hash);
  }
  fc::time_point openssl_end = fc::time_point::now();
  fc::microseconds openssl_duration = openssl_end - openssl_begin;
  uint32_t each = openssl_duration.count() / 10000;
  ilog("OpenSSL SHA256 raw calls, 64k bytes, 10,000 iterations: ${openssl_duration}μs, ${each}μs per", (openssl_duration)(each));
}

int main() {
  char buf[64 * 1024];
  fc::rand_bytes(buf, sizeof(buf));

  fc::ripemd160 legacy_ripemd160_result = fc::ripemd160::hash(buf, sizeof(buf));

  unsigned char btc_ripemd160_result[20];
  CRIPEMD160 hasher;
  hasher.Write((const unsigned char*)buf, sizeof(buf));
  hasher.Finalize(btc_ripemd160_result);
  FC_ASSERT(!memcmp(legacy_ripemd160_result.data(), btc_ripemd160_result, 20));

  test_openssl(buf, sizeof(buf));
  test_fc(buf, sizeof(buf));
  test_btc(buf, sizeof(buf));
  test_btc_reuse(buf, sizeof(buf));
  ilog("------------------------------------");

  test_openssl_sha256(buf, sizeof(buf));
  return 0;
}

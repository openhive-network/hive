#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hive { namespace siphash {

/// 128-bit SipHash key; plain values, so a hasher holding one stays valid when shared memory is mapped elsewhere
struct key
{
  uint64_t k0;
  uint64_t k1;
};

namespace detail {

inline uint64_t rotate_left( uint64_t x, int bits ) { return ( x << bits ) | ( x >> ( 64 - bits ) ); }

} // detail

/**
  * SipHash-1-3 of @p length bytes at @p data under @p k.
  *
  * Message words are read in host byte order, so the result matches the SipHash reference only on little-endian
  * hosts; callers use it for process-local bucket placement, never for anything sent or stored across nodes.
  */
inline uint64_t siphash13( const key& k, const void* data, size_t length )
{
  using detail::rotate_left;
  const unsigned char* bytes = static_cast< const unsigned char* >( data );
  uint64_t v0 = 0x736f6d6570736575ULL ^ k.k0;
  uint64_t v1 = 0x646f72616e646f6dULL ^ k.k1;
  uint64_t v2 = 0x6c7967656e657261ULL ^ k.k0;
  uint64_t v3 = 0x7465646279746573ULL ^ k.k1;
  const auto sip_round = [&]()
  {
    v0 += v1; v1 = rotate_left( v1, 13 ); v1 ^= v0; v0 = rotate_left( v0, 32 );
    v2 += v3; v3 = rotate_left( v3, 16 ); v3 ^= v2;
    v0 += v3; v3 = rotate_left( v3, 21 ); v3 ^= v0;
    v2 += v1; v1 = rotate_left( v1, 17 ); v1 ^= v2; v2 = rotate_left( v2, 32 );
  };
  const size_t whole_words_end = length & ~size_t( 7 );
  for( size_t i = 0; i < whole_words_end; i += 8 )
  {
    uint64_t word;
    std::memcpy( &word, bytes + i, sizeof( word ) );
    v3 ^= word;
    sip_round();
    v0 ^= word;
  }
  uint64_t last_word = uint64_t( length ) << 56;
  for( size_t i = whole_words_end; i < length; ++i )
    last_word |= uint64_t( bytes[i] ) << ( 8 * ( i - whole_words_end ) );
  v3 ^= last_word;
  sip_round();
  v0 ^= last_word;
  v2 ^= 0xff;
  sip_round();
  sip_round();
  sip_round();
  return v0 ^ v1 ^ v2 ^ v3;
}

} } // hive::siphash

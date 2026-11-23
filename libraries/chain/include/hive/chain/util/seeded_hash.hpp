#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hive { namespace chain { namespace util {

/**
  * SipHash-1-3 under a random key drawn when the object is constructed.
  *
  * Hashers of hashed indexes over author-chosen keys (permlinks, account names) hold one, so an author cannot pick
  * keys that fall into one bucket: placement depends on a key nobody else knows. A hasher is stored by value inside
  * its index, so an index in shared memory keeps its key across restart and reopen, while every newly created index
  * (new state, state rebuilt from a snapshot) gets a new one. Copies share the key of the original.
  *
  * Bucket order of such an index is therefore node-specific: it may only be searched, never iterated by anything
  * that affects consensus or any other output that has to be the same on every node.
  */
class seeded_hash
{
public:
  /// draws the key from a cryptographic random source; throws when that source fails
  seeded_hash();

  uint64_t operator()( const void* data, size_t length )const
  {
    const unsigned char* bytes = static_cast< const unsigned char* >( data );
    uint64_t v0 = 0x736f6d6570736575ULL ^ _k0;
    uint64_t v1 = 0x646f72616e646f6dULL ^ _k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ _k0;
    uint64_t v3 = 0x7465646279746573ULL ^ _k1;
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

private:
  static uint64_t rotate_left( uint64_t x, int bits ) { return ( x << bits ) | ( x >> ( 64 - bits ) ); }

  // plain values, so the hasher stays valid when shared memory is mapped at another address
  uint64_t _k0;
  uint64_t _k1;
};

} } } // hive::chain::util

#include <boost/test/unit_test.hpp>

#include <hive/siphash/siphash.hpp>

#include <cstdint>

namespace {

/// SipHash-1-3 of the messages (empty), 00, 00 01, ..., 00 01 ... 3e under the key 00 01 ... 0f: the inputs of the
/// SipHash reference vectors, with one compression and three finalization rounds (Rust's SipHasher13 test vectors)
const uint64_t reference_outputs[ 64 ] = {
  0xabac0158050fc4dcULL, 0xc9f49bf37d57ca93ULL, 0x82cb9b024dc7d44dULL, 0x8bf80ab8e7ddf7fbULL,
  0xcf75576088d38328ULL, 0xdef9d52f49533b67ULL, 0xc50d2b50c59f22a7ULL, 0xd3927d989bb11140ULL,
  0x369095118d299a8eULL, 0x25a48eb36c063de4ULL, 0x79de85ee92ff097fULL, 0x70c118c1f94dc352ULL,
  0x78a384b157b4d9a2ULL, 0x306f760c1229ffa7ULL, 0x605aa111c0f95d34ULL, 0xd320d86d2a519956ULL,
  0xcc4fdd1a7d908b66ULL, 0x9cf2689063dbd80cULL, 0x8ffc389cb473e63eULL, 0xf21f9de58d297d1cULL,
  0xc0dc2f46a6cce040ULL, 0xb992abfe2b45f844ULL, 0x7ffe7b9ba320872eULL, 0x525a0e7fdae6c123ULL,
  0xf464aeb267349c8cULL, 0x45cd5928705b0979ULL, 0x3a3e35e3ca9913a5ULL, 0xa91dc74e4ade3b35ULL,
  0xfb0bed02ef6cd00dULL, 0x88d93cb44ab1e1f4ULL, 0x540f11d643c5e663ULL, 0x2370dd1f8c21d1bcULL,
  0x81157b6c16a7b60dULL, 0x4d54b9e57a8ff9bfULL, 0x759f12781f2a753eULL, 0xcea1a3bebf186b91ULL,
  0x2cf508d3ada26206ULL, 0xb6101c2da3c33057ULL, 0xb3f47496ae3a36a1ULL, 0x626b57547b108392ULL,
  0xc1d2363299e41531ULL, 0x667cc1923f1ad944ULL, 0x65704ffec8138825ULL, 0x24f280d1c28949a6ULL,
  0xc2ca1cedfaf8876bULL, 0xc2164bfc9f042196ULL, 0xa16e9c9368b1d623ULL, 0x49fb169c8b5114fdULL,
  0x9f3143f8df074c46ULL, 0xc6fdaf2412cc86b3ULL, 0x7eaf49d10a52098fULL, 0x1cf313559d292f9aULL,
  0xc44a30dda2f41f12ULL, 0x36fae98943a71ed0ULL, 0x318fb34c73f0bce6ULL, 0xa27abf3670a7e980ULL,
  0xb4bcc0db243c6d75ULL, 0x23f8d852fdb71513ULL, 0x8f035f4da67d8a08ULL, 0xd89cd0e5b7e8f148ULL,
  0xf6f4e6bcf7a644eeULL, 0xaec59ad80f1837f2ULL, 0xc3b2f6154b6694e0ULL, 0x9d199062b7bbb3a8ULL
};

} // namespace

BOOST_AUTO_TEST_SUITE( siphash_tests )

BOOST_AUTO_TEST_CASE( siphash13_matches_reference_vectors )
{
  const hive::siphash::key key{ 0x0706050403020100ULL, 0x0f0e0d0c0b0a0908ULL };
  unsigned char message[ 64 ];
  for( size_t i = 0; i < sizeof( message ); ++i )
    message[ i ] = static_cast< unsigned char >( i );
  for( size_t length = 0; length < 64; ++length )
    BOOST_CHECK_MESSAGE( hive::siphash::siphash13( key, message, length ) == reference_outputs[ length ],
                         "length " << length << ": 0x" << std::hex << hive::siphash::siphash13( key, message, length ) );
}

BOOST_AUTO_TEST_CASE( siphash13_depends_on_the_key )
{
  const unsigned char message[] = "same message";
  const hive::siphash::key key_a{ 1, 2 }, key_b{ 1, 3 };
  BOOST_CHECK_NE( hive::siphash::siphash13( key_a, message, sizeof( message ) ),
                  hive::siphash::siphash13( key_b, message, sizeof( message ) ) );
  BOOST_CHECK_EQUAL( hive::siphash::siphash13( key_a, message, sizeof( message ) ),
                     hive::siphash::siphash13( hive::siphash::key{ 1, 2 }, message, sizeof( message ) ) );
}

BOOST_AUTO_TEST_SUITE_END()

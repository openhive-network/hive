/*
 * Copyright (c) 2026 Hive contributors.
 *
 * The MIT License
 */
#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/protocol/crypto_memo.hpp>

#include <fc/exception/exception.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/io/raw.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;

/**
 * Simulates the buggy memo_content format from commit 3af2709be (versions 1.27.5-1.28.6)
 * where fields were serialized as: nonce, check, encrypted, from, to
 * NOTE: Must be defined BEFORE BOOST_FIXTURE_TEST_SUITE to avoid FC_REFLECT creating
 * a local fc namespace that shadows the global ::fc namespace.
 */
struct memo_content_buggy_format
{
  uint64_t                      nonce = 0;
  uint32_t                      check = 0;
  std::vector<char>             encrypted;
  fc::ecc::public_key           from;
  fc::ecc::public_key           to;
};

FC_REFLECT( memo_content_buggy_format, (nonce)(check)(encrypted)(from)(to) )

/**
 * Test fixture providing pre-generated keys and common utilities for crypto_memo tests.
 * This reduces repetition across tests by providing ready-to-use key pairs.
 */
struct crypto_memo_fixture : public clean_database_fixture
{
  crypto_memo_fixture()
    : alice_key( generate_private_key( "alice_memo_key" ) )
    , bob_key( generate_private_key( "bob_memo_key" ) )
    , charlie_key( generate_private_key( "charlie_memo_key" ) )
  {}

  static fc::ecc::private_key generate_private_key( const std::string& seed )
  {
    return fc::ecc::private_key::regenerate( fc::sha256::hash( seed ) );
  }

  crypto_memo::key_finder_type make_key_finder() const
  {
    return make_key_finder( { alice_key, bob_key, charlie_key } );
  }

  static crypto_memo::key_finder_type make_key_finder( const std::vector<fc::ecc::private_key>& keys )
  {
    return [keys]( const fc::ecc::public_key& pub_key ) -> fc::optional<fc::ecc::private_key>
    {
      for( const auto& key : keys )
      {
        if( key.get_public_key() == pub_key )
          return key;
      }
      return fc::optional<fc::ecc::private_key>();
    };
  }

  /**
   * Helper to encrypt memo using the buggy format from versions 1.27.5-1.28.6.
   * The buggy format serialized fields as: (nonce)(check)(encrypted)(from)(to)
   * instead of the correct: (from)(to)(nonce)(check)(encrypted)
   */
  static std::string encrypt_memo_buggy_format(
    const fc::ecc::private_key& from_key,
    const fc::ecc::public_key& to_key,
    const std::string& memo,
    uint64_t nonce )
  {
    fc::crypto_data crypto;
    auto content = crypto.encrypt( from_key, to_key, memo, nonce );

    auto decoded = fc::from_base58( content );
    fc::crypto_data::content c;
    fc::raw::unpack_from_vector( decoded, c );

    memo_content_buggy_format buggy;
    buggy.nonce = c.nonce;
    buggy.check = c.check;
    buggy.encrypted = std::move( c.encrypted );
    buggy.from = from_key.get_public_key();
    buggy.to = to_key;

    return '#' + fc::to_base58( fc::raw::pack_to_vector( buggy ) );
  }

  fc::ecc::private_key alice_key;
  fc::ecc::private_key bob_key;
  fc::ecc::private_key charlie_key;
  crypto_memo memo_crypto;
};

BOOST_FIXTURE_TEST_SUITE( crypto_memo_tests, crypto_memo_fixture )

/**
 * Tests basic encrypt/decrypt roundtrip, serialization order, and format verification.
 * Verifies that:
 * - Encrypted memos start with '#' marker
 * - Decryption recovers the original message
 * - Serialization round-trips correctly via load_from_string/dump_to_string
 * - The correct field order is used (from, to, nonce, check, encrypted)
 */
BOOST_AUTO_TEST_CASE( crypto_memo_encoding_roundtrip )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: crypto_memo encoding roundtrip and format verification" );

    std::string original_memo = "#This is a secret memo for testing";
    uint64_t fixed_nonce = 12345678901234567890ULL;

    // Encrypt the memo
    std::string encrypted = memo_crypto.encrypt( alice_key, bob_key.get_public_key(), original_memo, fixed_nonce );

    // Basic format checks
    BOOST_CHECK( encrypted.size() > 0 );
    BOOST_CHECK( encrypted[0] == '#' );

    // Verify decryption works
    std::string decrypted = memo_crypto.decrypt( make_key_finder(), encrypted );
    BOOST_CHECK_EQUAL( decrypted, original_memo.substr(1) );

    // Verify serialization roundtrip
    auto content = memo_crypto.load_from_string( encrypted );
    BOOST_REQUIRE( content.has_value() );
    BOOST_CHECK( content->from == alice_key.get_public_key() );
    BOOST_CHECK( content->to == bob_key.get_public_key() );
    BOOST_CHECK_EQUAL( content->nonce, fixed_nonce );

    std::string re_serialized = memo_crypto.dump_to_string( content.value() );
    BOOST_CHECK_EQUAL( encrypted, re_serialized );

    // Verify correct field order: first bytes should be 'from' public key
    auto decoded = fc::from_base58( encrypted.substr(1) );
    BOOST_REQUIRE( decoded.size() >= 33 );
    uint8_t first_byte = static_cast<uint8_t>( decoded[0] );
    // Compressed public keys start with 0x02 or 0x03
    BOOST_CHECK_MESSAGE( first_byte == 0x02 || first_byte == 0x03,
      "Encrypted memo should start with 'from' public key (0x02 or 0x03), got: " + std::to_string(first_byte) );

    // Verify deterministic encryption with same nonce
    std::string encrypted2 = memo_crypto.encrypt( alice_key, bob_key.get_public_key(), original_memo, fixed_nonce );
    BOOST_CHECK_EQUAL( encrypted, encrypted2 );

    // Different nonce should produce different result
    std::string encrypted3 = memo_crypto.encrypt( alice_key, bob_key.get_public_key(), original_memo, fixed_nonce + 1 );
    BOOST_CHECK( encrypted != encrypted3 );

    BOOST_TEST_MESSAGE( "Encoding roundtrip test passed" );
  }
  catch( const ::fc::exception& ) { throw; }
}

/**
 * Regression test for buggy memo format from versions 1.27.5-1.28.6.
 * The buggy format serialized fields as: (nonce)(check)(encrypted)(from)(to)
 * This test verifies that memos encrypted with the buggy format can still be decrypted.
 */
BOOST_AUTO_TEST_CASE( crypto_memo_decrypt_buggy_format )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: crypto_memo decrypt buggy format (versions 1.27.5-1.28.6)" );

    std::string original_message = "Secret message in buggy format";
    uint64_t fixed_nonce = 1111222233334444555ULL;

    // Encrypt using the buggy format
    std::string buggy_encrypted = encrypt_memo_buggy_format(
      alice_key, bob_key.get_public_key(), original_message, fixed_nonce );

    BOOST_CHECK( buggy_encrypted.size() > 0 );
    BOOST_CHECK( buggy_encrypted[0] == '#' );

    // Encrypt the same message with current (correct) format
    std::string current_encrypted = memo_crypto.encrypt(
      alice_key, bob_key.get_public_key(), "#" + original_message, fixed_nonce );

    // The encrypted strings should be different (different serialization order)
    BOOST_CHECK( buggy_encrypted != current_encrypted );

    // But both should decrypt to the same message
    std::string decrypted_current = memo_crypto.decrypt( make_key_finder(), current_encrypted );
    std::string decrypted_buggy = memo_crypto.decrypt( make_key_finder(), buggy_encrypted );

    BOOST_CHECK_EQUAL( decrypted_current, original_message );
    BOOST_CHECK_EQUAL( decrypted_buggy, original_message );

    BOOST_TEST_MESSAGE( "Buggy format decryption test passed" );
  }
  catch( const ::fc::exception& ) { throw; }
}

/**
 * Tests encryption/decryption with various message types including unicode and special chars.
 */
BOOST_AUTO_TEST_CASE( crypto_memo_various_messages )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: crypto_memo with various message types" );

    std::vector<std::string> messages = {
      "#Short",
      "#A slightly longer memo message",
      "#Unicode test: \xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF\xE4\xB8\x96\xE7\x95\x8C",
      "#Numbers: 12345.67890",
      "#Special chars: !@#$%^&*()_+-=[]{}|;':\",./<>?"
    };

    uint64_t nonce = 1000000000000000000ULL;

    for( const auto& msg : messages )
    {
      // Alice to Bob
      std::string encrypted = memo_crypto.encrypt( alice_key, bob_key.get_public_key(), msg, nonce++ );
      std::string decrypted = memo_crypto.decrypt( make_key_finder(), encrypted );
      BOOST_CHECK_EQUAL( decrypted, msg.substr(1) );

      // Bob to Charlie
      encrypted = memo_crypto.encrypt( bob_key, charlie_key.get_public_key(), msg, nonce++ );
      decrypted = memo_crypto.decrypt( make_key_finder(), encrypted );
      BOOST_CHECK_EQUAL( decrypted, msg.substr(1) );
    }

    BOOST_TEST_MESSAGE( "Various messages test passed" );
  }
  catch( const ::fc::exception& ) { throw; }
}

/**
 * Tests error handling: wrong key returns original, invalid format returns original.
 */
BOOST_AUTO_TEST_CASE( crypto_memo_error_handling )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: crypto_memo error handling" );

    // Test wrong key - should return encrypted memo unchanged
    std::string original_memo = "#Secret between Alice and Bob only";
    uint64_t fixed_nonce = 7777888899990000111ULL;

    std::string encrypted = memo_crypto.encrypt( alice_key, bob_key.get_public_key(), original_memo, fixed_nonce );

    // Key finder that only knows Charlie's key (not Alice or Bob)
    auto wrong_key_finder = make_key_finder( { charlie_key } );

    std::string result = memo_crypto.decrypt( wrong_key_finder, encrypted );
    BOOST_CHECK_EQUAL( result, encrypted );

    // Test memo without '#' marker - should return unchanged
    std::string no_marker = "no hash marker";
    BOOST_CHECK_EQUAL( memo_crypto.decrypt( make_key_finder(), no_marker ), no_marker );

    BOOST_TEST_MESSAGE( "Error handling test passed" );
  }
  catch( const ::fc::exception& ) { throw; }
}

BOOST_AUTO_TEST_SUITE_END()
#endif

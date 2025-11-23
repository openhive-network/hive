#include <hive/protocol/hive_specialised_exceptions.hpp>
#include <hive/protocol/crypto_memo.hpp>

namespace hive { namespace protocol {

crypto_memo::memo_content crypto_memo::build_from_encrypted_content( const crypto_data::public_key_type& from, const crypto_data::public_key_type& to, crypto_data::content&& content )
{
  memo_content _result{ from, to, content.nonce, content.check, std::move( content.encrypted ) };

  return _result;
}

crypto_memo::memo_content crypto_memo::build_from_base58_content( const crypto_data::public_key_type& from, const crypto_data::public_key_type& to, const std::string& content )
{
  auto _c = from_string_impl<crypto_data::content>( content );
  HIVE_PROTOCOL_VALIDATION_ASSERT( _c, "Build from `base58` content failed", ("content", content ) );
  return build_from_encrypted_content( from, to, std::move( _c.value() ) );
}

std::optional<crypto_memo::memo_content> crypto_memo::load_from_string( const std::string& data )
{
  HIVE_PROTOCOL_VALIDATION_ASSERT( data.size() > 0, "Failed to load crypto memo from string: empty data" );
  HIVE_PROTOCOL_VALIDATION_ASSERT( data[0] == marker, "Failed to load crypto memo from string: `${data}`",
    ("data", data)("first_char_in_data", data[0])("marker", marker)
  );
  return from_string_impl<memo_content>( data.substr(1) );
}

std::optional<crypto_memo::memo_content> crypto_memo::load_from_string_buggy_format( const std::string& data )
{
  // Defensive check - protects against future refactoring that might call this directly
  if( data.empty() || data[0] != marker )
    return std::optional<memo_content>();

  auto buggy = from_string_impl<memo_content_buggy_format>( data.substr(1) );
  if( !buggy )
    return std::optional<memo_content>();
  // Convert buggy format to current format
  return memo_content{ buggy->from, buggy->to, buggy->nonce, buggy->check, std::move( buggy->encrypted ) };
}

std::string crypto_memo::dump_to_string( const memo_content& content )
{
  return marker + to_string_impl( content );
}

std::string crypto_memo::encrypt( const crypto_data::private_key_type& from, const crypto_data::public_key_type& to, const std::string& memo, std::optional<uint64_t> nonce /*= std::optional<uint64_t>()*/ )
{
  HIVE_PROTOCOL_VALIDATION_ASSERT( memo.size() > 0, "Failed to encrypt memo: empty memo" );
  HIVE_PROTOCOL_VALIDATION_ASSERT( memo[0] == marker, "Failed to encrypt memo: `${memo}`",
    ("memo", memo)("first_char_in_memo", memo[0])("marker", marker)
  );
  return dump_to_string( build_from_encrypted_content( from.get_public_key(), to, encrypt_impl( from, to, memo.substr(1), nonce ) ) );
}

std::string crypto_memo::decrypt( key_finder_type key_finder, const std::string& encrypted_memo )
{
  try
  {
    // Try current format first (from, to, nonce, check, encrypted)
    auto _c = load_from_string( encrypted_memo );
    if( _c )
    {
      auto _result = decrypt_impl( key_finder, _c->from, _c->to, { _c->nonce, _c->check, std::move( _c->encrypted ) } );
      if( _result.has_value() )
        return _result.value();
      // Correct format parsed successfully but decryption failed (wrong key) - return original
      // Don't try buggy format as it would misinterpret correctly-formatted data
      return encrypted_memo;
    }

    // Current format parsing failed - try buggy format (nonce, check, encrypted, from, to)
    // This handles memos encrypted with the broken version from commit 3af2709be (versions 1.27.5-1.28.6)
    auto _c_buggy = load_from_string_buggy_format( encrypted_memo );
    if( _c_buggy )
    {
      auto _result = decrypt_impl( key_finder, _c_buggy->from, _c_buggy->to, { _c_buggy->nonce, _c_buggy->check, std::move( _c_buggy->encrypted ) } );
      if( _result.has_value() )
        return _result.value();
    }
  }
  catch( ... )
  {
    // If parsing or decryption fails for any reason, return original memo unchanged
  }

  return encrypted_memo;
}

} } // hive::protocol

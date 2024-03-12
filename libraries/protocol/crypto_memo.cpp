#include <hive/protocol/crypto_memo.hpp>

namespace hive { namespace protocol {

crypto_memo::memo_content crypto_memo::build_from_encrypted_content( const crypto_data::public_key_type& from, const crypto_data::public_key_type& to, crypto_data::content&& content )
{
  memo_content _result( std::move( content ) );
  _result.from = from;
  _result.to = to;

  return _result;
}

crypto_memo::memo_content crypto_memo::build_from_base58_content( const crypto_data::public_key_type& from, const crypto_data::public_key_type& to, const std::string& content )
{
  auto _c = from_string_impl<crypto_data::content>( content );
  if( !_c )
    FC_ASSERT( false, "Build from `base58` content failed");

  return build_from_encrypted_content( from, to, std::move( _c.value() ) );
}

std::optional<crypto_memo::memo_content> crypto_memo::load_from_string( const std::string& data )
{
  FC_ASSERT( data.size() > 0 && data[0] == marker );
  return from_string_impl<memo_content>( data.substr(1) );
}

std::string crypto_memo::dump_to_string( const memo_content& content )
{
  return marker + to_string_impl( content );
}

std::string crypto_memo::encrypt( const crypto_data::private_key_type& from, const crypto_data::public_key_type& to, const std::string& memo )
{
  FC_ASSERT( memo.size() > 0 && memo[0] == marker );
  return dump_to_string( build_from_encrypted_content( from.get_public_key(), to, encrypt_impl( from, to, memo.substr(1) ) ) );
}

std::string crypto_memo::decrypt( key_finder_type key_finder, const std::string& encrypted_memo )
{
  auto _c = load_from_string( encrypted_memo );
  if( !_c )
    return encrypted_memo;

  auto _result = decrypt_impl( key_finder, _c->from, _c->to, _c.value() );
  if( !_result.has_value() )
    return encrypted_memo;

  return _result.value();
}

} } // hive::protocol

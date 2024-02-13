#include <hive/protocol/crypto_memo.hpp>

namespace hive { namespace protocol {

std::string crypto_memo::encrypt( const crypto_data::private_key_type& from, const crypto_data::public_key_type& to, const std::string& memo )
{
  FC_ASSERT( memo.size() > 0 && memo[0] == marker );
  content _c = encrypt_impl( from, to, memo.substr(1) );

  memo_content _result;
  _result.check = _c.check;
  _result.nonce = _c.nonce;
  _result.encrypted = std::move( _c.encrypted );
  _result.from = from.get_public_key();
  _result.to = to;

  return marker + to_string_impl( _result );
}

std::string crypto_memo::decrypt( key_finder_type key_finder, const std::string& encrypted_memo )
{
  FC_ASSERT( encrypted_memo.size() > 0 && encrypted_memo[0] == marker );

  auto _c = from_string_impl<memo_content>( encrypted_memo.substr(1) );
  if( !_c )
    return encrypted_memo;

  auto _result = decrypt_impl( key_finder, _c->from, _c->to, _c.value() );
  if( !_result.has_value() )
    return encrypted_memo;

  return _result.value();
}

} } // hive::protocol

#include <fc/crypto/crypto_data.hpp>

#include <fc/crypto/aes.hpp>

namespace fc {

fc::sha512 crypto_data::generate_encrypted_key( uint64_t nonce, const fc::sha512& shared_secret )
{
  fc::sha512::encoder enc;
  fc::raw::pack( enc, nonce );
  fc::raw::pack( enc, shared_secret );
  return enc.result();
}

crypto_data::content crypto_data::encrypt_impl( const crypto_data::private_key_type& from, const crypto_data::public_key_type& to, const std::string& data, std::optional<uint64_t> nonce /*= std::optional<uint64_t>()*/)
{
  content _c;

  _c.nonce = nonce.value_or(fc::time_point::now().time_since_epoch().count());

  auto shared_secret = from.get_shared_secret( to );

  auto _encrypt_key = generate_encrypted_key( _c.nonce, shared_secret );

  _c.encrypted = fc::aes_encrypt( _encrypt_key, fc::raw::pack_to_vector( data ) );
  _c.check = fc::sha256::hash( _encrypt_key )._hash[0];

  return _c;
}

std::string crypto_data::encrypt( const crypto_data::private_key_type& from, const crypto_data::public_key_type& to, const std::string& data, std::optional<uint64_t> nonce /*= std::optional<uint64_t>()*/ )
{
  content _result = encrypt_impl( from, to, data, nonce );
  return to_string_impl( _result );
}

std::optional<std::string> crypto_data::decrypt_impl( key_finder_type key_finder,
                                                      const crypto_data::public_key_type& from, const crypto_data::public_key_type& to,
                                                      const content& encrypted_content )
{
  fc::sha512 _shared_secret;

  auto _from_key = key_finder( from );
  if( !_from_key )
  {
    auto _to_key = key_finder( to );
    if( !_to_key )
      return std::optional<std::string>();
    _shared_secret = _to_key->get_shared_secret( from );
  }
  else
  {
    _shared_secret = _from_key->get_shared_secret( to );
  }

  auto _encryption_key = generate_encrypted_key( encrypted_content.nonce, _shared_secret );

  uint32_t _check = fc::sha256::hash( _encryption_key )._hash[0];
  if( _check != encrypted_content.check )
    return std::optional<std::string>();

  try
  {
    std::vector<char> _decrypted = fc::aes_decrypt( _encryption_key, encrypted_content.encrypted );
    std::string _decrypted_string;
    fc::raw::unpack_from_vector( _decrypted, _decrypted_string );
    return _decrypted_string;
  }
  catch( ... ) {}

  return std::optional<std::string>();
}

std::string crypto_data::decrypt( key_finder_type key_finder,
                                  const crypto_data::public_key_type& from, const crypto_data::public_key_type& to,
                                  const std::string& encrypted_data )
{
  auto _c = from_string_impl<content>( encrypted_data );
  if( !_c )
    FC_ASSERT( false, "Decryption failed");

  auto _result = decrypt_impl( key_finder, from, to, _c.value() );
  if( !_result.has_value() )
    FC_ASSERT( false, "Decryption failed");

  return _result.value();
}

}// fc

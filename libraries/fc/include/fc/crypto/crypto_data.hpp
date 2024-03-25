#pragma once

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/io/raw.hpp>

namespace fc {

class crypto_data
{
  public:

    using private_key_type  = fc::ecc::private_key;
    using public_key_type   = fc::ecc::public_key;

    using key_finder_type = std::function<fc::optional<crypto_data::private_key_type>( const crypto_data::public_key_type& )>;

    struct content
    {
      uint64_t          nonce = 0;
      uint32_t          check = 0;
      std::vector<char> encrypted;
    };

  private:

    fc::sha512 generate_encrypted_key( uint64_t nonce, const fc::sha512& shared_secret );

  protected:

    template<typename ContentType>
    std::optional<ContentType> from_string_impl( const std::string& encrypted_data )
    {
      try
      {
        auto data = fc::from_base58( encrypted_data );
        ContentType _c;
        fc::raw::unpack_from_vector( data, _c );
        FC_ASSERT( to_string_impl( _c ) == encrypted_data );
        return _c;
      }
      catch( ... ) {}
      return std::optional<ContentType>();
    }

    template<typename ContentType>
    std::string to_string_impl( const ContentType& data )
    {
      return fc::to_base58( fc::raw::pack_to_vector( data ) );
    }

    content encrypt_impl( const crypto_data::private_key_type& from, const crypto_data::public_key_type& to, const std::string& data );
    std::optional<std::string> decrypt_impl( key_finder_type key_finder, const crypto_data::public_key_type& from, const crypto_data::public_key_type& to, const content& encrypted_content );

  public:

    virtual ~crypto_data() = default;

    /**
     * Encrypts given data
     */
    std::string encrypt( const crypto_data::private_key_type& from, const crypto_data::public_key_type& to, const std::string& data );

    /**
     * Decrypts given data.
     */
    std::string decrypt( key_finder_type key_finder, const crypto_data::public_key_type& from, const crypto_data::public_key_type& to, const std::string& encrypted_data );
};

}//fc

FC_REFLECT( fc::crypto_data::content, (nonce)(check)(encrypted) )

#pragma once

#include <fc/crypto/crypto_data.hpp>

namespace hive { namespace protocol {

using crypto_data = fc::crypto_data;

class crypto_memo: public crypto_data
{
  public:

    struct memo_content: public content
    {
      public_key_type   from;
      public_key_type   to;
    };

  private:

    const char marker = '#';

  protected:

  public:

    ~crypto_memo() override = default;

    /**
     * Encrypts given memo
     */
    std::string encrypt( const crypto_data::private_key_type& from, const crypto_data::public_key_type& to, const std::string& data );

    /**
     * Decrypts given memo (it has to start with #).
     */
    std::string decrypt( key_finder_type key_finder, const std::string& encrypted_data );
};

} } // hive::protocol

FC_REFLECT_DERIVED( hive::protocol::crypto_memo::memo_content, (fc::crypto_data::content), (from)(to) )

#pragma once

#include <core/utilities.hpp>

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <optional>

namespace beekeeper {

class beekeeper_api final
{
  private:
    class impl;

    class impl_deleter
    {
    public:
      void operator()(beekeeper_api::impl* ptr) const;
    };

    const std::string empty_response;

    bool initialized = false;

    std::unique_ptr<impl, impl_deleter> _impl;

    template<typename T>
    std::string to_string( const T& src, bool valid = true );
    std::string extend_json( const std::string& src );

    std::string exception_handler( std::function<std::string()>&& method, std::function<void(bool)>&& aux_init_method = std::function<void(bool)>() );

    std::string create_impl( const std::string& token, const std::string& wallet_name, const std::optional<std::string>& password );
    std::string get_public_keys_impl( const std::string& token, const std::optional<std::string>& wallet_name );
    std::string sign_digest_impl( const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::optional<std::string>& wallet_name );

  public:

    beekeeper_api( const std::vector<std::string>& _params );

    std::string init();

    std::string create_session( const std::string& salt );
    std::string close_session( const std::string& token );

    std::string create( const std::string& token, const std::string& wallet_name );
    std::string create( const std::string& token, const std::string& wallet_name, const std::string& password );

    std::string unlock( const std::string& token, const std::string& wallet_name, const std::string& password );
    std::string open( const std::string& token, const std::string& wallet_name );
    std::string close( const std::string& token, const std::string& wallet_name );

    std::string set_timeout( const std::string& token, seconds_type seconds );

    std::string lock_all( const std::string& token );
    std::string lock( const std::string& token, const std::string& wallet_name );

    std::string import_key( const std::string& token, const std::string& wallet_name, const std::string& wif_key );
    std::string remove_key( const std::string& token, const std::string& wallet_name, const std::string& password, const std::string& public_key );

    std::string list_wallets( const std::string& token );

    std::string get_public_keys( const std::string& token );
    std::string get_public_keys( const std::string& token, const std::string& wallet_name );

    std::string sign_digest( const std::string& token, const std::string& sig_digest, const std::string& public_key );
    std::string sign_digest( const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::string& wallet_name );

    std::string get_info( const std::string& token );

    std::string has_matching_private_key( const std::string& token, const std::string& wallet_name, const std::string& public_key );

    std::string encrypt_data( const std::string& token, const std::string& from_public_key, const std::string& to_public_key, const std::string& content );
    std::string decrypt_data( const std::string& token, const std::string& from_public_key, const std::string& to_public_key, const std::string& encrypted_content );
};

}

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
  public:
    struct init_data
    {
      uint32_t status = false;
      std::string version;
    };

  private:
    class impl;

    class impl_deleter
    {
    public:
      void operator()(beekeeper_api::impl* ptr) const;
    };

    const std::string empty_response;

    bool initialized = false;
  
    /*
      HIVE_ADDRESS_PREFIX from protocol/config.hpp is not accessible for WASM beekeeper so here a duplicate is defined.
      At now this is only one allowed prefix, but maybe in the future custom prefixes could be used as well.
    */
    const char* prefix = "STM";

    std::unique_ptr<impl, impl_deleter> _impl;

    public_key_type create( const std::string& source );

    template<typename T>
    std::string to_string( const T& src, bool valid = true );
    std::string extend_json( const std::string& src );

    std::string exception_handler( std::function<std::string()>&& method, std::function<void(bool)>&& aux_init_method = std::function<void(bool)>() );

    std::string create_session_impl( const std::optional<std::string>& salt );

    std::string create_impl( const std::string& token, const std::string& wallet_name, const std::optional<std::string>& password, const bool is_temporary );
    std::string get_public_keys_impl( const std::string& token, const std::optional<std::string>& wallet_name );
    std::string sign_digest_impl( const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::optional<std::string>& wallet_name );

  public:

    beekeeper_api( const std::vector<std::string>& _params );

    std::string init();

    std::string create_session();
    std::string create_session( const std::string& salt );
    std::string close_session( const std::string& token );

    std::string create( const std::string& token, const std::string& wallet_name );
    std::string create( const std::string& token, const std::string& wallet_name, const std::string& password );
    std::string create( const std::string& token, const std::string& wallet_name, const std::string& password, bool is_temporary );

    std::string unlock( const std::string& token, const std::string& wallet_name, const std::string& password );
    std::string open( const std::string& token, const std::string& wallet_name );
    std::string close( const std::string& token, const std::string& wallet_name );

    std::string set_timeout( const std::string& token, seconds_type seconds );

    std::string lock_all( const std::string& token );
    std::string lock( const std::string& token, const std::string& wallet_name );

    std::string import_key( const std::string& token, const std::string& wallet_name, const std::string& wif_key );
    std::string import_keys( const std::string& token, const std::string& wallet_name, const std::vector<std::string>& wif_keys );
    std::string remove_key( const std::string& token, const std::string& wallet_name, const std::string& public_key );

    std::string list_wallets( const std::string& token );

    std::string get_public_keys( const std::string& token );
    std::string get_public_keys( const std::string& token, const std::string& wallet_name );

    std::string sign_digest( const std::string& token, const std::string& sig_digest, const std::string& public_key );
    std::string sign_digest( const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::string& wallet_name );

    std::string get_info( const std::string& token );
    std::string get_version();

    std::string has_matching_private_key( const std::string& token, const std::string& wallet_name, const std::string& public_key );

    std::string encrypt_data( const std::string& token, const std::string& from_public_key, const std::string& to_public_key, const std::string& wallet_name, const std::string& content );
    std::string encrypt_data( const std::string& token, const std::string& from_public_key, const std::string& to_public_key, const std::string& wallet_name, const std::string& content, unsigned int nonce );
    std::string decrypt_data( const std::string& token, const std::string& from_public_key, const std::string& to_public_key, const std::string& wallet_name, const std::string& encrypted_content );

    std::string has_wallet( const std::string& token, const std::string& wallet_name );

    std::string is_wallet_unlocked( const std::string& token, const std::string& wallet_name );
};

}

namespace fc
{
  void to_variant( const beekeeper::beekeeper_api::init_data& var, fc::variant& vo );
  void from_variant( const variant& var, beekeeper::beekeeper_api::init_data& vo );
}
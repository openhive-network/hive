#pragma once

#include <core/utilities.hpp>

#include <memory>
#include <string>
#include <vector>
#include <functional>

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

    std::unique_ptr<impl, impl_deleter> _impl;

    template<typename T>
    std::string to_string( const T& src, bool valid = true );
    std::string extend_json( const std::string& src );

    std::string exception_handler( std::function<std::string()>&& method );

  public:

    beekeeper_api( const std::vector<std::string>& _params );

    std::string init();

    std::string create_session( const std::string& salt );
    std::string close_session( const std::string& token );

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

    std::string sign_digest( const std::string& token, const std::string& public_key, const std::string& sig_digest );
    std::string sign_binary_transaction( const std::string& token, const std::string& transaction, const std::string& chain_id, const std::string& public_key );
    std::string sign_transaction( const std::string& token, const std::string& transaction, const std::string& chain_id, const std::string& public_key );

    std::string get_info( const std::string& token );
};

}

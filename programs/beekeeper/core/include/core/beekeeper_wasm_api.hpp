#pragma once

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

    std::unique_ptr<impl, impl_deleter> _impl;

    template<typename T>
    std::string to_string( const T& src );

    template<typename result_type>
    result_type exception_handler( std::function<result_type()>&& method );

  public:

    beekeeper_api( const std::vector<std::string>& _params );

    std::string init();

    std::string create_session( const std::string& salt );
    void close_session( const std::string& token );

    std::string create( const std::string& token, const std::string& wallet_name, const std::string& password );
    void unlock( const std::string& token, const std::string& wallet_name, const std::string& password );
    void open( const std::string& token, const std::string& wallet_name );
    void close( const std::string& token, const std::string& wallet_name );

    void set_timeout( const std::string& token, int32_t seconds );

    void lock_all( const std::string& token );
    void lock( const std::string& token, const std::string& wallet_name );

    std::string import_key( const std::string& token, const std::string& wallet_name, const std::string& wif_key );
    void remove_key( const std::string& token, const std::string& wallet_name, const std::string& password, const std::string& public_key );

    std::string list_wallets( const std::string& token );
    std::string get_public_keys( const std::string& token );

    std::string sign_digest( const std::string& token, const std::string& public_key, const std::string& sig_digest );
    std::string sign_binary_transaction( const std::string& token, const std::string& transaction, const std::string& chain_id, const std::string& public_key );
    std::string sign_transaction( const std::string& token, const std::string& transaction, const std::string& chain_id, const std::string& public_key );

    std::string get_info( const std::string& token );
};

}
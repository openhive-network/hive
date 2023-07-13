#pragma once

#include <beekeeper_wasm_app_api/beekeeper_wasm_app.hpp>

#include <fc/io/json.hpp>

#include<vector>

namespace beekeeper {

class beekeeper_api
{
  private:

    std::vector<std::string> params;

    beekeeper::beekeeper_wasm_app app;

    template<typename T>
    std::string get_string( const T& src )
    {
      fc::variant _v;
      fc::to_variant( src, _v );
      return fc::json::to_string( _v );
    }

    template<typename T>
    std::vector<std::string> convert_to_vector(const T& src )
    {
      std::vector<std::string> _result;

      for( auto& item : src )
        _result.emplace_back( get_string( item ) );

      return _result;
    }

  public:

    beekeeper_api( const std::vector<std::string>& _params );

    int init();

    std::string create_session( const std::string& salt );
    void close_session( const std::string& token );

    std::string create( const std::string& token, const std::string& wallet_name, const std::string& password );
    void unlock( const std::string& token, const std::string& wallet_name, const std::string& password );
    void open( const std::string& token, const std::string& wallet_name );
    void close( const std::string& token, const std::string& wallet_name );

    void set_timeout( const std::string& token, int64_t seconds );

    void lock_all( const std::string& token );
    void lock( const std::string& token, const std::string& wallet_name );

    std::string import_key( const std::string& token, const std::string& wallet_name, const std::string& wif_key );
    void remove_key( const std::string& token, const std::string& wallet_name, const std::string& password, const std::string& public_key );

    std::vector<std::string> list_wallets( const std::string& token );
    std::vector<std::string> get_public_keys( const std::string& token );

    std::string sign_digest( const std::string& token, const std::string& public_key, const std::string& sig_digest );
    std::string sign_transaction( const std::string& token, const std::string& transaction, const std::string& chain_id, const std::string& public_key, const std::string& sig_digest );

    std::string get_info( const std::string& token );
};

}
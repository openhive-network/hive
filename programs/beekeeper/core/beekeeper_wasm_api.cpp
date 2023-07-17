#include <core/beekeeper_wasm_api.hpp>

#include <core/utilities.hpp>

#include <fc/optional.hpp>

namespace beekeeper {

  beekeeper_api::beekeeper_api( const std::vector<std::string>& _params ): params( _params )
  {
    params.insert( params.begin(), "path to exe" );
  }

  int beekeeper_api::init()
  {
    char** _params = nullptr;
    int _result = 1;

    try
    {
      _params = new char*[ params.size() ];
      for( size_t i = 0; i < params.size(); ++i )
        _params[i] = static_cast<char*>( params[i].data() );

      _result = app.init( params.size(), _params );

      if( _params )
        delete[] _params;
    }
    catch(...)
    {
      if( _params )
        delete[] _params;
    }

    return _result;
  }

  std::string beekeeper_api::create_session( const std::string& salt )
  {
    create_session_return _result{ app.get_wallet_manager()->create_session( salt, "notification endpoint"/*notifications_endpoint - not used here*/ ) };
    return to_string( _result );
  }

  void beekeeper_api::close_session( const std::string& token )
  {
    app.get_wallet_manager()->close_session( token );
  }

  std::string beekeeper_api::create( const std::string& token, const std::string& wallet_name, const std::string& password )
  {
    create_return _result;

    if( password.empty() )
      _result = { app.get_wallet_manager()->create( token, wallet_name ) };
    else
      _result = { app.get_wallet_manager()->create( token, wallet_name, password ) };

    return to_string( _result );
  }

  void beekeeper_api::unlock( const std::string& token, const std::string& wallet_name, const std::string& password )
  {
    app.get_wallet_manager()->unlock( token, wallet_name, password );
  }

  void beekeeper_api::open( const std::string& token, const std::string& wallet_name )
  {
    app.get_wallet_manager()->open( token, wallet_name );
  }

  void beekeeper_api::close( const std::string& token, const std::string& wallet_name )
  {
    app.get_wallet_manager()->close( token, wallet_name );
  }

  void beekeeper_api::set_timeout( const std::string& token, int32_t seconds )
  {
    app.get_wallet_manager()->set_timeout( token, seconds );
  }

  void beekeeper_api::lock_all( const std::string& token )
  {
    app.get_wallet_manager()->lock_all( token );
  }

  void beekeeper_api::lock( const std::string& token, const std::string& wallet_name )
  {
    app.get_wallet_manager()->lock( token, wallet_name );
  }

  std::string beekeeper_api::import_key( const std::string& token, const std::string& wallet_name, const std::string& wif_key )
  {
    import_key_return _result = { app.get_wallet_manager()->import_key( token, wallet_name, wif_key ) };
    return to_string( _result );
  }

  void beekeeper_api::remove_key( const std::string& token, const std::string& wallet_name, const std::string& password, const std::string& public_key )
  {
    app.get_wallet_manager()->remove_key( token, wallet_name, password, public_key );
  }

  std::string beekeeper_api::list_wallets( const std::string& token )
  {
    list_wallets_return _result = { app.get_wallet_manager()->list_wallets( token ) };
    return to_string( _result );
  }

  std::string beekeeper_api::get_public_keys( const std::string& token )
  {
    get_public_keys_return _result = { utility::get_public_keys( app.get_wallet_manager()->get_public_keys( token ) ) };
    return to_string( _result );
  }

  std::string beekeeper_api::sign_digest( const std::string& token, const std::string& public_key, const std::string& sig_digest )
  {
    signature_return _result = { app.get_wallet_manager()->sign_digest( token, utility::get_public_key( public_key ), digest_type( sig_digest ) ) };
    return to_string( _result );
  }

  std::string beekeeper_api::sign_transaction( const std::string& token, const std::string& transaction, const std::string& chain_id, const std::string& public_key, const std::string& sig_digest )
  {
    signature_return _result = { app.get_wallet_manager()->sign_transaction( token, transaction, chain_id_type( chain_id ), utility::get_public_key( public_key ), digest_type( sig_digest ) ) };
    return to_string( _result );
  }

  std::string beekeeper_api::get_info( const std::string& token )
  {
    return to_string( app.get_wallet_manager()->get_info( token ) );
  }

};

#include <beekeeper_wasm_app_api/beekeeper_wasm_api.hpp>

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
      for( auto i = 0; i < params.size(); ++i )
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
    return app.get_wallet_manager()->create_session( salt, "notification endpoint"/*notifications_endpoint - not used here*/ );
  }

};

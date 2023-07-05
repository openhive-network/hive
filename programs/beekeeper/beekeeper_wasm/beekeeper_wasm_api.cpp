#include <beekeeper_wasm_core/beekeeper_wasm_app.hpp>

#include <emscripten/bind.h>

using namespace emscripten;

class beekeeper_api
{
  private:

    std::vector<std::string> params;

    beekeeper::beekeeper_wasm_app app;

  public:

    beekeeper_api( const std::vector<std::string>& _params ): params( _params )
    {
      params.insert( params.begin(), "path to exe" );
    }

    int init()
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

    std::string create_session( const std::string& salt )
    {
      return app.get_wallet_manager()->create_session( salt, "notification endpoint"/*notifications_endpoint - not used here*/ );
    }

};

EMSCRIPTEN_BINDINGS(beekeeper_api) {
  class_<beekeeper_api>("beekeeper_api")
    .constructor<std::vector<std::string>>()
    .function("init", &beekeeper_api::init)
    .function("create_session", &beekeeper_api::create_session)
    ;
}

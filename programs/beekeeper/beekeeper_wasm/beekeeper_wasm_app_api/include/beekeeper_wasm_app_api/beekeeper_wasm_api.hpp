#pragma once

#include <beekeeper_wasm_app_api/beekeeper_wasm_app.hpp>

#include<vector>

namespace beekeeper {

class beekeeper_api
{
  private:

    std::vector<std::string> params;

    beekeeper::beekeeper_wasm_app app;

  public:

    beekeeper_api( const std::vector<std::string>& _params );

    int init();

    std::string create_session( const std::string& salt );
};

}
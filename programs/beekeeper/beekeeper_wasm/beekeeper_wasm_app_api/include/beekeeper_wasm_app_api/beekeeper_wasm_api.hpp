#pragma once

#include <beekeeper_wasm_app_api/beekeeper_wasm_app.hpp>

#include <fc/optional.hpp>

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
    std::string create( const std::string& token, const std::string& wallet_name, const std::string& password );
    std::string import_key( const std::string& token, const std::string& wallet_name, const std::string& wif_key );
};

}
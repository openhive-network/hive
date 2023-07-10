#include <beekeeper_wasm_app_api/beekeeper_wasm_api.hpp>

#include<iostream>

int main()
{
  beekeeper::beekeeper_api _obj( { "--wallet-dir", "./beekeeper-storage/", "--salt", "avocado" } );
  auto _init_result = _obj.init();

  if( _init_result == 1 )
  {
    auto _token = _obj.create_session( "banana" );
    std::cout<<_token<<std::endl;
  }

  return 0;
}

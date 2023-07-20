#include <beekeeper_wasm_app_api/beekeeper_wasm_api.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include<iostream>

int main()
{
  try
  {
    beekeeper::beekeeper_api _obj( { "--wallet-dir", "./beekeeper-storage/", "--salt", "avocado" } );
    auto _init_result = _obj.init();

    if( _init_result == 1 )
    {
      auto _token = _obj.create_session( "banana" );
      std::cout<<"token: "<<_token<<std::endl;

      auto _password = _obj.create( _token, "wallet_b", "" );
      std::cout<<"_password: "<<_password<<std::endl;

      _password = _obj.create( _token, "wallet_a", "cherry" );
      std::cout<<"_password: "<<_password<<std::endl;

      auto _public_key = _obj.import_key( _token, "wallet_a", "5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78" );
      std::cout<<"_public_key: "<<_public_key<<std::endl;

      _public_key = _obj.import_key( _token, "wallet_a", "5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS" );
      std::cout<<"_public_key: "<<_public_key<<std::endl;
    }
  }
  catch ( const boost::exception& e )
  {
    std::cerr << boost::diagnostic_information(e) << "\n";
  }
  catch ( const fc::exception& e )
  {
    std::cerr << e.to_detail_string() << "\n";
  }
  catch ( const std::exception& e )
  {
    std::cerr << e.what() << "\n";
  }
  catch ( ... )
  {
    std::cerr << "unknown exception\n";
  }

  return 0;
}

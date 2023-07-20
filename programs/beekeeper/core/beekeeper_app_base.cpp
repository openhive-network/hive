#include <core/beekeeper_app_base.hpp>

#include <boost/exception/diagnostic_information.hpp>

namespace beekeeper {

int beekeeper_app_base::run( int argc, char** argv )
{
  set_program_options();

  auto _init_app_result = initialize( argc, argv );
  if( _init_app_result.second )
    return _init_app_result.first;

  return start();
}

}

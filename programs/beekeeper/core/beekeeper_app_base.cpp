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

int beekeeper_app_base::execute( p_beekeeper_app_base&& app, int argc, char** argv )
{
  try
  {
    FC_ASSERT( app );
    return app->run( argc, argv );
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

  return -1;
}

}

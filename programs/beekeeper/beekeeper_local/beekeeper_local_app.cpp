#include <beekeeper_local/beekeeper_local_app.hpp>

namespace beekeeper {

beekeeper_local_app::beekeeper_local_app()
{
}

beekeeper_local_app::~beekeeper_local_app()
{
}

std::pair<appbase::initialization_result::result, bool> beekeeper_local_app::initialize( int argc, char** argv )
{
  app.add_program_options( bpo::options_description(), options );
  app.set_app_name( "beekeeper" );

  auto initializationResult = app.initialize<>( argc, argv );
  init_status = static_cast<appbase::initialization_result::result>( initializationResult.get_result_code() );

  if( !initializationResult.should_start_loop() )
    return { init_status, true };
  else
  {
    auto _initialization = initialize_program_options();
    if( !_initialization.first )
      return { init_status, true };

    hive::notify_hived_status( "starting with token: " + _initialization.second );
    return { appbase::initialization_result::result::ok, false };
  }
}

appbase::initialization_result::result beekeeper_local_app::start()
{
  app.startup();
  app.exec();
  std::cout << "exited cleanly\n";

  return init_status;
}

}
#include <beekeeper/beekeeper_app.hpp>

#include <hive/plugins/webserver/webserver_plugin.hpp>

namespace beekeeper {

beekeeper_app::beekeeper_app()
{
}

beekeeper_app::~beekeeper_app()
{
}

std::pair<appbase::initialization_result::result, bool> beekeeper_app::initialize( int argc, char** argv )
{
  app.add_program_options( bpo::options_description(), options );
  app.set_app_name( "beekeeper" );

  app.register_plugin<hive::plugins::webserver::webserver_plugin>();

  auto initializationResult = app.initialize<
                                hive::plugins::webserver::webserver_plugin >
                              ( argc, argv );
  init_status = static_cast<appbase::initialization_result::result>( initializationResult.get_result_code() );

  if( !initializationResult.should_start_loop() )
    return { init_status, true };
  else
  {
    auto _initialization = initialize_program_options();
    if( !_initialization.first )
      return { init_status, true };

    api_ptr = std::make_unique<beekeeper::beekeeper_wallet_api>( wallet_manager_ptr );

    hive::notify_hived_status( "starting with token: " + _initialization.second );
    return { appbase::initialization_result::result::ok, false };
  }
}

appbase::initialization_result::result beekeeper_app::start()
{
  auto& _webserver_plugin = app.get_plugin<hive::plugins::webserver::webserver_plugin>();

  webserver_connection = _webserver_plugin.add_connection( [this](const hive::utilities::notifications::collector_t& collector){ wallet_manager_ptr->save_connection_details( collector ); } );

  _webserver_plugin.start_webserver();

  app.startup();
  app.exec();
  std::cout << "exited cleanly\n";

  return init_status;
}

}
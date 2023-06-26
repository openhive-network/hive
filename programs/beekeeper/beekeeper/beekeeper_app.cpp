#include <beekeeper/beekeeper_app.hpp>
#include <beekeeper/session_manager.hpp>

#include <core/beekeeper_wallet_manager.hpp>

#include <hive/plugins/webserver/webserver_plugin.hpp>

namespace beekeeper {

beekeeper_app::beekeeper_app(): app( appbase::app() )
{
}

beekeeper_app::~beekeeper_app()
{
}

void beekeeper_app::set_program_options()
{
  hive::utilities::notifications::add_program_options( options );

  beekeeper_app_init::set_program_options();
}

std::pair<bool, bool> beekeeper_app::initialize( int argc, char** argv )
{
  app.add_program_options( bpo::options_description(), options );
  app.set_app_name( "beekeeper" );

  app.register_plugin<hive::plugins::webserver::webserver_plugin>();

  auto initializationResult = app.initialize<
                                hive::plugins::webserver::webserver_plugin >
                              ( argc, argv );
  init_status = static_cast<appbase::initialization_result::result>( initializationResult.get_result_code() );

  if( !initializationResult.should_start_loop() )
    return { init_status == appbase::initialization_result::result::ok, true };
  else
  {
    auto _initialization = initialize_program_options();
    if( !_initialization.first )
      return { init_status, true };

    api_ptr = std::make_unique<beekeeper::beekeeper_wallet_api>( wallet_manager_ptr );

    appbase::app().notify_status( "starting with token: " + _initialization.second );
    return { true, false };
  }
}

bool beekeeper_app::start()
{
  auto& _webserver_plugin = app.get_plugin<hive::plugins::webserver::webserver_plugin>();

  webserver_connection = _webserver_plugin.add_connection( [this](const hive::utilities::notifications::collector_t& collector )
  {
    instance->save_connection_details( collector );
  } );

  _webserver_plugin.start_webserver();

  app.startup();
  app.exec();
  std::cout << "exited cleanly\n";

  return init_status == appbase::initialization_result::result::ok;
}

const boost::program_options::variables_map& beekeeper_app::get_args() const
{
  return app.get_args();
}

bfs::path beekeeper_app::get_data_dir() const
{
  return app.data_dir();
}

void beekeeper_app::setup_notifications( const boost::program_options::variables_map& args )
{
  app.setup_notifications( args );
}

std::shared_ptr<beekeeper::beekeeper_wallet_manager> beekeeper_app::create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit )
{
  instance = std::make_shared<beekeeper_instance>( cmd_wallet_dir );
  return std::make_shared<beekeeper::beekeeper_wallet_manager>( std::make_shared<session_manager>(), instance,
                                                                       cmd_wallet_dir, cmd_unlock_timeout, cmd_session_limit );
}

}
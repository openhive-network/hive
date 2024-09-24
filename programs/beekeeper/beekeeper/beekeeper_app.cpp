#include <beekeeper/beekeeper_app.hpp>
#include <beekeeper/time_manager.hpp>

#include <core/beekeeper_wallet_manager.hpp>

#include <fc/value_set.hpp>

#include <hive/plugins/webserver/webserver_plugin.hpp>
#include <hive/plugins/app_status_api/app_status_api_plugin.hpp>

#include <boost/scope_exit.hpp>

namespace beekeeper {

beekeeper_app::beekeeper_app()
{
  app.init_signals_handler();
}

beekeeper_app::~beekeeper_app()
{
  if( start_loop )
  {
    ilog("beekeeper is exiting");
    app.quit( true/*log*/ );
    ilog("exited cleanly");
  }
  else
    app.quit();
}

void beekeeper_app::set_program_options()
{
  options_cli.add_options()
    ("export-keys-wallet", boost::program_options::value< std::vector<std::string> >()->composing()->multitoken(),
      "Export explicitly private keys to a local file `wallet_name.keys`. Both [name, password] are required for every wallet. By default is empty."
      "Two wallets example: --export-keys-wallet \"[\"blue-wallet\", \"PW5JViFn5gd4rt6ohk7DQMgHzQN6Z9FuMRfKoE5Ysk25mkjy5AY1b\"]\" --export-keys-wallet \"[\"green-wallet\", \"PW5KYF9Rt4ETnuP4uheHSCm9kLbCuunf6RqeKgQ8QRoxZmGeZUhhk\"]\" ")
    ;

  options_cfg.add_options()
    ("unlock-interval", boost::program_options::value<uint64_t>()->default_value( 500 ), "Protection against unlocking by bots. Every wrong `unlock` enables a delay. By default 500[ms]." )
    ;

  beekeeper_app_base::set_program_options();
}

std::string beekeeper_app::check_version()
{
  std::string _version = "{\"version\":\"";
  _version += utility::get_revision();
  _version += "\"}";

  return _version;
}

struct keys_container
{
  std::string public_key;
  std::string private_key;
};

bool beekeeper_app::save_keys( const std::string& wallet_name, const std::string& wallet_password )
{
  bool _result = true;

  if( wallet_name.empty() || wallet_password.empty() )
    return _result;

  const std::string _filename = wallet_name + ".keys";

  ilog( "*****Saving keys into `${_filename}` file*****", (_filename) );

  ilog( "Create a session" );
  std::string _token = wallet_manager_ptr->create_session( "salt" );

  auto _save_keys = [&]()
  {
    ilog( "Unlock the wallet" );
    wallet_manager_ptr->unlock( _token, wallet_name, wallet_password );

    ilog( "Get keys" );
    auto _keys = wallet_manager_ptr->list_keys( _token, wallet_name, wallet_password );

    std::vector<keys_container> _v;
    std::transform( _keys.begin(), _keys.end(), std::back_inserter( _v ),
    []( const beekeeper::key_detail_pair& item )
    {
      return keys_container{ beekeeper::utility::public_key::to_string( item ), item.second.first.key_to_wif() };
    } );
    
    ilog( "Save keys into `${_filename}` file", (_filename) );
    fc::path _file( _filename );
    fc::json::save_to_file( _v, _file );
  };

  auto _finish = [this, &_token, &wallet_name]()
  {
    ilog( "Lock the wallet" );
    wallet_manager_ptr->lock( _token, wallet_name );

    ilog( "Close a session" );
    wallet_manager_ptr->close_session( _token, false/*allow_close_all_sessions_action*/ );
  };

  auto _exec_action = [&_filename, &_result]( std::function<void()>&& call )
  {
    try
    {
      call();
    }
    catch ( const boost::exception& e )
    {
      _result = false;
      elog( boost::diagnostic_information(e) );
    }
    catch ( const fc::exception& e )
    {
      _result = false;
      elog( e.to_detail_string() );
    }
    catch ( const std::exception& e )
    {
      _result = false;
      elog( e.what() );
    }
    catch ( ... )
    {
      _result = false;
      elog( "Unknown error" );
    }
  };

  BOOST_SCOPE_EXIT(&wallet_manager_ptr, &_exec_action, &_finish, &_result)
  {
    _exec_action( _finish );

    if ( _result )
      ilog( "*****Keys have been saved*****" );
    else
      elog( "*****Saving keys failed*****" );

  } BOOST_SCOPE_EXIT_END

  _exec_action( _save_keys );

  return _result;
}

init_data beekeeper_app::initialize( int argc, char** argv )
{
  app.add_program_options( options_cli, options_cfg );
  app.set_app_name( "beekeeper" );
  app.set_version_string( check_version() );

  app.register_plugin<hive::plugins::webserver::webserver_plugin>();
  app.register_plugin<hive::plugins::app_status_api::app_status_api_plugin>();

  auto initializationResult = app.initialize<
                                hive::plugins::webserver::webserver_plugin,
                                hive::plugins::app_status_api::app_status_api_plugin>
                              ( argc, argv );
  start_loop = initializationResult.should_start_loop();

  if( !initializationResult.should_start_loop() )
    return { initializationResult.get_result_code() == appbase::initialization_result::ok, "" };
  else
  {
    auto _initialization = initialize_program_options();
    if( !_initialization.status )
    {
      start_loop = false;
      return _initialization;
    }

    if( instance->is_instance_started() )
    {
      api_ptr = std::make_unique<beekeeper::beekeeper_wallet_api>( wallet_manager_ptr, app, unlock_interval );
      instance->get_app().save_status( "beekeeper is starting", "beekeeper_status" );
    }

    return _initialization;
  }
}

void beekeeper_app::start()
{
  auto& _webserver_plugin = app.get_plugin<hive::plugins::webserver::webserver_plugin>();

  app.startup();

  //Launch webserver only when all plugins are initialized at startup.
  if( !app.is_interrupt_request() )
  {
    _webserver_plugin.start_webserver();
    instance->get_app().save_status( "beekeeper is ready", "beekeeper_status" );
  }

  ilog("beekeeper is waiting");
  app.wait( true/*log*/ );
  ilog("waiting is finished");
}

const boost::program_options::variables_map& beekeeper_app::get_args() const
{
  return app.get_args();
}

bfs::path beekeeper_app::get_data_dir() const
{
  return app.data_dir();
}

void beekeeper_app::setup( const boost::program_options::variables_map& args )
{
  FC_ASSERT( args.count("unlock-interval") );
  unlock_interval = args.at("unlock-interval").as<uint64_t>();
}

init_data beekeeper_app::save_keys( const boost::program_options::variables_map& args )
{
  bool _result = true;

  using _strings_pair_type = std::pair< std::string, std::string >;
  fc::flat_map< std::string, std::string > _items;
  fc::load_value_set<_strings_pair_type>( args, "export-keys-wallet", _items );

  for( auto& item : _items )
  {
    _result = save_keys( item.first, item.second );
    if( !_result )
      break;
  }

  if( args.count("export-keys-wallet") )
    start_loop = false;

  return { _result, utility::get_revision() };
}

std::shared_ptr<beekeeper::beekeeper_wallet_manager> beekeeper_app::create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit )
{
  instance = std::make_shared<beekeeper_instance>( app, cmd_wallet_dir );
  return std::make_shared<beekeeper::beekeeper_wallet_manager>( std::make_shared<session_manager_base>( std::make_shared<time_manager>() ), instance,
                                                                       cmd_wallet_dir, cmd_unlock_timeout, cmd_session_limit,
                                                                       [this]() { app.kill(); } );
}

bool beekeeper_app::should_start_loop() const
{
  return start_loop;
};

init_data beekeeper_app::init( int argc, char** argv )
{
  try
  {
    return run( argc, argv );
  }
  catch ( const boost::exception& e )
  {
    elog(boost::diagnostic_information(e));
  }
  catch ( const fc::exception& e )
  {
    elog(e.to_detail_string());
  }
  catch ( const std::exception& e )
  {
    elog(e.what());
  }
  catch ( ... )
  {
    elog("unknown exception");
  }

  return init_data();
}

}

namespace fc
{
  void to_variant( const beekeeper::keys_container& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "public_key", var.public_key )( "private_key", var.private_key );
    vo = v;
  }
}

FC_REFLECT( beekeeper::keys_container, (public_key)(private_key) )

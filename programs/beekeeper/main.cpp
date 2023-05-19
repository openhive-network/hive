#include <appbase/application.hpp>

#include <hive/plugins/webserver/webserver_plugin.hpp>

#include <beekeeper/beekeeper_wallet_manager.hpp>
#include <beekeeper/beekeeper_wallet_api.hpp>

#include <fc/exception/exception.hpp>
#include <fc/thread/thread.hpp>
#include <fc/interprocess/signals.hpp>
#include <fc/git_revision.hpp>
#include <fc/stacktrace.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <csignal>
#include <vector>

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;

class beekeeper_app
{
    bpo::options_description                              options;
    std::unique_ptr<beekeeper::beekeeper_wallet_manager>  wallet_manager_ptr;
    std::unique_ptr<beekeeper::beekeeper_wallet_api>      api_ptr;

    appbase::application&                   app;
    appbase::initialization_result::result  init_status = appbase::initialization_result::result::ok;

    void set_program_options()
    {
      hive::utilities::notifications::add_program_options( options );

      options.add_options()
            ("wallet-dir", bpo::value<boost::filesystem::path>()->default_value("."),
              "The path of the wallet files (absolute path or relative to application data dir)")

            ("unlock-timeout", bpo::value<int64_t>()->default_value(900),
              "Timeout for unlocked wallet in seconds (default 900 (15 minutes)). "
              "Wallets will automatically lock after specified number of seconds of inactivity. "
              "Activity is defined as any wallet command e.g. list-wallets.")

            ("salt", bpo::value<std::string>()->default_value(""),
              "Random data that is used as an additional input so as to create token")

            ("backtrace", bpo::value<std::string>()->default_value( "yes" ), "Whether to print backtrace on SIGSEGV" )
            ;
    }

    std::pair<bool, std::string> initialize_program_options()
    {
      ilog("initializing options");
      try {
          const boost::program_options::variables_map& _args = app.get_args();
          hive::utilities::notifications::setup_notifications( _args );

          std::string _notification;
          if( _args.count("notifications-endpoint") )
          {
            auto _notifications = _args.at("notifications-endpoint").as<std::vector<std::string>>();
            if( !_notifications.empty() )
              _notification = *_notifications.begin();
          }

          FC_ASSERT( _args.count("wallet-dir") );
          auto _dir = _args.at("wallet-dir").as<boost::filesystem::path>();
          if(_dir.is_relative() )
              _dir = app.data_dir() / _dir;
          if( !bfs::exists( _dir ) )
              bfs::create_directories( _dir );

          if( !wallet_manager_ptr->start( _dir ) )
            return { false, "" };

          FC_ASSERT( _args.count("unlock-timeout") );
          auto _timeout = _args.at("unlock-timeout").as<int64_t>();
          FC_ASSERT( _timeout > 0, "Please specify a positive timeout ${t}", ("t", _timeout));
          std::chrono::seconds _t( _timeout );

          FC_ASSERT( _args.count("salt") );
          auto _salt = _args.at("salt").as<std::string>();

          auto _token = wallet_manager_ptr->create_session( _salt, _notification );
          wallet_manager_ptr->set_timeout( _token, _t );

          FC_ASSERT( _args.count("backtrace") );
          if( _args.at( "backtrace" ).as<std::string>() == "yes" )
          {
            fc::print_stacktrace_on_segfault();
            ilog( "Backtrace on segfault is enabled." );
          }

          return { true, _token };
      } FC_LOG_AND_RETHROW()
    }

    std::pair<appbase::initialization_result::result, bool> initialize( int argc, char** argv )
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
        wallet_manager_ptr  = std::make_unique<beekeeper::beekeeper_wallet_manager>();
        api_ptr             = std::make_unique<beekeeper::beekeeper_wallet_api>( get_wallet_manager() );

        auto _initialization = initialize_program_options();
        if( !_initialization.first )
          return { init_status, true };

        hive::notify_hived_status( "starting with token: " + _initialization.second );
        return { appbase::initialization_result::result::ok, false };
      }
    }

    appbase::initialization_result::result start()
    {
      auto& _webserver_plugin = app.get_plugin<hive::plugins::webserver::webserver_plugin>();

      webserver_connection = _webserver_plugin.add_connection( [this](const hive::utilities::notifications::collector_t& collector){ wallet_manager_ptr->save_connection_details( collector ); } );

      _webserver_plugin.start_webserver();

      app.startup();
      app.exec();
      std::cout << "exited cleanly\n";

      return init_status;
    }

  public:

    beekeeper_app(): app( appbase::app() )
    {
    }

    ~beekeeper_app()
    {
      if( webserver_connection.connected() )
        webserver_connection.disconnect();
    }

    beekeeper::beekeeper_wallet_manager& get_wallet_manager()
    {
      return *wallet_manager_ptr;
    }

    int run( int argc, char** argv )
    {
      set_program_options();

      auto _init_app_result = initialize( argc, argv );
      if( _init_app_result.second )
        return _init_app_result.first;

      return start();
    }
    
    boost::signals2::connection webserver_connection;
};

int main( int argc, char** argv )
{
  try
  {
    beekeeper_app _beekeeper_app;
    return _beekeeper_app.run( argc, argv );
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

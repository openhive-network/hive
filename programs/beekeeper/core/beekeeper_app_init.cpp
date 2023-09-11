#include <core/beekeeper_app_init.hpp>

#include <fc/io/json.hpp>
#include <fc/stacktrace.hpp>
#include <fc/git_revision.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/filesystem.hpp>
#include <boost/scope_exit.hpp>

namespace bfs = boost::filesystem;

namespace beekeeper {

beekeeper_app_init::beekeeper_app_init()
{
}

beekeeper_app_init::~beekeeper_app_init()
{
}

void beekeeper_app_init::set_program_options()
{
  options.add_options()
    ("wallet-dir", bpo::value<boost::filesystem::path>()->default_value("."),
      "The path of the wallet files (absolute path or relative to application data dir)")

    ("unlock-timeout", bpo::value<uint64_t>()->default_value(900),
      "Timeout for unlocked wallet in seconds (default 900 (15 minutes)). "
      "Wallets will automatically lock after specified number of seconds of inactivity. "
      "Activity is defined as any wallet command e.g. list-wallets.")

    ("export-keys-wallet-name", bpo::value<std::string>()->default_value(""),
      "Export explicitly private keys to a local file `wallet_name.keys`. Both (name/password) are required. By default is empty." )

    ("export-keys-wallet-password", bpo::value<std::string>()->default_value(""),
      "Export explicitly private keys to a local file `wallet_name.keys`. Both (name/password) are required. By default is empty." )

    ("backtrace", bpo::value<std::string>()->default_value( "yes" ), "Whether to print backtrace on SIGSEGV" )
    ;
}

bool beekeeper_app_init::save_keys( const std::string& notification, const std::string& wallet_name, const std::string& wallet_password )
{
  bool _result = true;

  if( wallet_name.empty() || wallet_password.empty() )
    return _result;

  std::string _token = wallet_manager_ptr->create_session( "salt", notification );

  const std::string _filename = wallet_name + ".keys";

  ilog("Try to save keys into `${_filename}` file.", (_filename));

  auto _save_keys = [&]()
  {
    wallet_manager_ptr->unlock( _token, wallet_name, wallet_password );
    auto _keys = wallet_manager_ptr->list_keys( _token, wallet_name, wallet_password );

    map<std::string, std::string> _result;
    std::transform( _keys.begin(), _keys.end(), std::inserter( _result, _result.end() ),
    []( const std::pair<beekeeper::public_key_type, beekeeper::private_key_type>& item )
    {
      return std::make_pair( beekeeper::utility::public_key::to_string( item.first ), item.second.key_to_wif() );
    } );
    
    fc::path _file( _filename );
    fc::json::save_to_file( _result, _file );

    ilog( "Keys have been saved.", (_filename) );
  };

  auto _finish = [this, &_token, &wallet_name]()
  {
    wallet_manager_ptr->lock( _token, wallet_name );
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
      wlog( boost::diagnostic_information(e) );
    }
    catch ( const fc::exception& e )
    {
      _result = false;
      wlog( e.to_detail_string() );
    }
    catch ( const std::exception& e )
    {
      _result = false;
      wlog( e.what() );
    }
    catch ( ... )
    {
      _result = false;
      wlog( "Unknown error. Saving keys into a `${_filename}` file failed.",(_filename) );
    }
  };

  BOOST_SCOPE_EXIT(&wallet_manager_ptr, &_exec_action, &_finish)
  {
    _exec_action( _finish );
  } BOOST_SCOPE_EXIT_END

  _exec_action( _save_keys );

  return _result;
}

std::string beekeeper_app_init::get_notifications_endpoint( const boost::program_options::variables_map& args )
{
  std::string _notification;

  if( args.count("notifications-endpoint") )
  {
    auto _notifications = args.at("notifications-endpoint").as<std::vector<std::string>>();
    if( !_notifications.empty() )
      _notification = *_notifications.begin();
  }

  return _notification;
}

init_data beekeeper_app_init::initialize_program_options()
{
  ilog("initializing options");
  try {
      const boost::program_options::variables_map& _args = get_args();
      setup_notifications( _args );

      std::string _notification = get_notifications_endpoint( _args );

      FC_ASSERT( _args.count("wallet-dir") );
      auto _dir = _args.at("wallet-dir").as<boost::filesystem::path>();
      if(_dir.is_relative() )
          _dir = get_data_dir() / _dir;
      if( !bfs::exists( _dir ) )
          bfs::create_directories( _dir );

      FC_ASSERT( _args.count("unlock-timeout") );
      auto _timeout = _args.at("unlock-timeout").as<uint64_t>();

      wallet_manager_ptr = create_wallet( _dir, _timeout, session_limit );

      if( !wallet_manager_ptr->start() )
        return { false, "" };

      FC_ASSERT( _args.count("backtrace") );
      if( _args.at( "backtrace" ).as<std::string>() == "yes" )
      {
        fc::print_stacktrace_on_segfault();
        ilog( "Backtrace on segfault is enabled." );
      }

      FC_ASSERT( _args.count("export-keys-wallet-name") );
      FC_ASSERT( _args.count("export-keys-wallet-password") );
      bool _result = save_keys( _notification, _args.at( "export-keys-wallet-name" ).as<std::string>(), _args.at( "export-keys-wallet-password" ).as<std::string>() );

      return { _result, fc::git_revision_sha };
  } FC_LOG_AND_RETHROW()
}

std::string beekeeper_app_init::check_version()
{
  std::string _version = "{\"version\":\"";
  _version += fc::git_revision_sha;
  _version += "\"}";

  return _version;
}

init_data beekeeper_app_init::init( int argc, char** argv )
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

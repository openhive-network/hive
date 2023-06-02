#include <core/beekeeper_app_init.hpp>

#include <fc/io/json.hpp>
#include <fc/stacktrace.hpp>

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
  hive::utilities::notifications::add_program_options( options );

  options.add_options()
    ("wallet-dir", bpo::value<boost::filesystem::path>()->default_value("."),
      "The path of the wallet files (absolute path or relative to application data dir)")

    ("unlock-timeout", bpo::value<uint64_t>()->default_value(900),
      "Timeout for unlocked wallet in seconds (default 900 (15 minutes)). "
      "Wallets will automatically lock after specified number of seconds of inactivity. "
      "Activity is defined as any wallet command e.g. list-wallets.")

    ("salt", bpo::value<std::string>()->default_value(""),
      "Random data that is used as an additional input so as to create token")

    ("export-keys-wallet-name", bpo::value<std::string>()->default_value(""),
      "Export explicitly private keys to a local file `wallet_name.keys`. Both (name/password) are required. By default is empty." )

    ("export-keys-wallet-password", bpo::value<std::string>()->default_value(""),
      "Export explicitly private keys to a local file `wallet_name.keys`. Both (name/password) are required. By default is empty." )

    ("backtrace", bpo::value<std::string>()->default_value( "yes" ), "Whether to print backtrace on SIGSEGV" )
    ;
}

void beekeeper_app_init::save_keys( const std::string& token, const std::string& wallet_name, const std::string& wallet_password )
{
  if( token.empty() || wallet_name.empty() || wallet_password.empty() )
    return;

  const std::string _filename = wallet_name + ".keys";

  ilog("Try to save keys into `${_filename}` file.", (_filename));

  BOOST_SCOPE_EXIT(&wallet_manager_ptr, &token, &wallet_name)
  {
    wallet_manager_ptr->lock( token, wallet_name );
  } BOOST_SCOPE_EXIT_END

  try
  {
    wallet_manager_ptr->unlock( token, wallet_name, wallet_password );
    auto _keys = wallet_manager_ptr->list_keys( token, wallet_name, wallet_password );

    map<std::string, std::string> _result;
    std::transform( _keys.begin(), _keys.end(), std::inserter( _result, _result.end() ),
    []( const std::pair<beekeeper::public_key_type, beekeeper::private_key_type>& item )
    {
      return std::make_pair( beekeeper::public_key_type::to_base58( item.first, false/*is_sha256*/ ), item.second.key_to_wif() );
    } );
    
    fc::path _file( _filename );
    fc::json::save_to_file( _result, _file );

    ilog( "Keys have been saved.", (_filename) );
    return;
  }
  catch ( const boost::exception& e )
  {
    wlog( boost::diagnostic_information(e) );
  }
  catch ( const fc::exception& e )
  {
    wlog( e.to_detail_string() );
  }
  catch ( const std::exception& e )
  {
    wlog( e.what() );
  }
  catch ( ... )
  {
    wlog( "Unknown error. Saving keys into a `${_filename}` file failed.",(_filename) );
  }
  wlog( "A problem occured during saving keys into `${_filename}` file.", (_filename) );
}

std::pair<bool, std::string> beekeeper_app_init::initialize_program_options()
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

      FC_ASSERT( _args.count("unlock-timeout") );
      auto _timeout = _args.at("unlock-timeout").as<uint64_t>();

      wallet_manager_ptr  = std::make_shared<beekeeper::beekeeper_wallet_manager>( _dir, _timeout, session_limit );

      if( !wallet_manager_ptr->start() )
        return { false, "" };

      FC_ASSERT( _args.count("salt") );
      auto _salt = _args.at("salt").as<std::string>();

      auto _token = wallet_manager_ptr->create_session( _salt, _notification );

      FC_ASSERT( _args.count("backtrace") );
      if( _args.at( "backtrace" ).as<std::string>() == "yes" )
      {
        fc::print_stacktrace_on_segfault();
        ilog( "Backtrace on segfault is enabled." );
      }

      FC_ASSERT( _args.count("export-keys-wallet-name") );
      FC_ASSERT( _args.count("export-keys-wallet-password") );
      save_keys( _token, _args.at( "export-keys-wallet-name" ).as<std::string>(), _args.at( "export-keys-wallet-password" ).as<std::string>() );

      return { true, _token };
  } FC_LOG_AND_RETHROW()
}

}
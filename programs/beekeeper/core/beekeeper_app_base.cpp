#include <core/beekeeper_app_base.hpp>

#include <fc/io/json.hpp>
#include <fc/stacktrace.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/filesystem.hpp>

namespace bfs = boost::filesystem;

namespace beekeeper {

beekeeper_app_base::beekeeper_app_base()
{
}

beekeeper_app_base::~beekeeper_app_base()
{
}

void beekeeper_app_base::set_program_options()
{
  options_cfg.add_options()
  /*
    The option `wallet-dir` can't have `boost::filesystem::path` type.
    Explanation:
      https://stackoverflow.com/questions/68716288/q-boost-program-options-using-stdfilesystempath-as-option-fails-when-the-gi
  */
    ("wallet-dir", bpo::value<std::string>()->default_value("."),
      "The path of the wallet files (absolute path or relative to application data dir)")

    ("unlock-timeout", bpo::value<uint64_t>()->default_value(900),
      "Timeout for unlocked wallet in seconds (default 900 (15 minutes))."
      "Wallets will be automatically locked after specified number of seconds of inactivity."
      "Activity is defined as any wallet command e.g. list-wallets.")

    ("backtrace", bpo::value<std::string>()->default_value( "yes" ), "Whether to print backtrace on SIGSEGV" )
    ;
}

uint32_t beekeeper_app_base::initialize_program_options()
{
  try {
      const boost::program_options::variables_map& _args = get_args();

      setup( _args );
      ilog("Options are set.");

      FC_ASSERT( _args.count("wallet-dir") );
      boost::filesystem::path _dir( _args.at("wallet-dir").as<std::string>() );
      if(_dir.is_relative() )
          _dir = get_data_dir() / _dir;
      if( !bfs::exists( _dir ) )
          bfs::create_directories( _dir );

      FC_ASSERT( _args.count("unlock-timeout") );
      auto _timeout = _args.at("unlock-timeout").as<uint64_t>();

      wallet_manager_ptr = create_wallet( _dir, _timeout, session_limit );

      FC_ASSERT( _args.count("backtrace") );
      if( _args.at( "backtrace" ).as<std::string>() == "yes" )
      {
        fc::print_stacktrace_on_segfault();
        ilog( "Backtrace on segfault is enabled." );
      }

      wallet_manager_ptr->start();

      return save_keys( _args );

  } FC_LOG_AND_RETHROW()
}

uint32_t beekeeper_app_base::run( int argc, char** argv )
{
  set_program_options();

  auto _result = initialize( argc, argv );

  if( should_start_loop() )
    start();

  return _result;
}

}
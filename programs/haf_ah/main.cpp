#include <hive/protocol/protocol.hpp>
#include <hive/protocol/types.hpp>

#include <hive/sql_utilities/transaction_controllers.hpp>

#include <fc/string.hpp>
#include <fc/variant.hpp>
#include <fc/static_variant.hpp>
#include <fc/io/json.hpp>

#include <fstream>
#include <streambuf>
#include <chrono>
#include <csignal>
#include <atomic>

#include <boost/program_options.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/format.hpp>

#include <pqxx/pqxx>

namespace bpo = boost::program_options;

using fc::string;

using hive::utilities::transaction_controller;
using hive::utilities::transaction_controller_ptr;

using transaction_ptr = transaction_controller::transaction_ptr;

struct ah_query
{
  using part_query_type = std::array<string, 3>;

  const string application_context;

  string create_context;
  string detach_context;
  string attach_context;
  string check_context;

  string next_block;

  string query_executor;

  template<typename T>
  string format( const string& src, const T& arg )
  {
    return boost::str( boost::format( src ) % arg );
  }

  template<typename T1, typename T2>
  string format( const string& src, const T1& arg, const T2& arg2 )
  {
    return boost::str( boost::format( src ) % arg % arg2 );
  }

  template<typename T1, typename T2, typename T3>
  string format( const string& src, const T1& arg, const T2& arg2, const T3& arg3 )
  {
    return boost::str( boost::format( src ) % arg % arg2 % arg3 );
  }

  template<typename T1, typename T2, typename T3, typename T4>
  string format( const string& src, const T1& arg, const T2& arg2, const T3& arg3, const T4& arg4 )
  {
    return boost::str( boost::format( src ) % arg % arg2 % arg3 % arg4 );
  }

  string create_full_query( const string& q_parts0, const string& q_parts1, const string& q_parts2 )
  {
    return q_parts0 + q_parts1 + q_parts2;
  }

  ah_query( const string& _application_context ) : application_context( _application_context )
  {
    create_context  = format( "SELECT * FROM hive.app_create_context('%s');", application_context );
    detach_context  = format( "SELECT * FROM hive.app_context_detach('%s');", application_context );
    attach_context  = "SELECT * FROM hive.app_context_attach('%s', %d);";
    check_context   = format( "SELECT * FROM hive.app_context_exists('%s');", application_context );

    next_block      = format( "SELECT * FROM hive.app_next_block('%s');", application_context );

    query_executor  = "SELECT * FROM public.fill_accounts_operations( %d, %d );";
  }
};

struct args_container
{
  string    url;
  fc::path  schema_path;
  uint64_t  flush_size    = 0;
};

class ah_loader
{
  public:

    using ah_loader_ptr = std::shared_ptr<ah_loader>;

  private:

    std::atomic_bool interrupted;

    args_container args;

    const string application_context = "account_history";

    ah_query query;

    transaction_ptr trx;
    transaction_controller_ptr tx_controller;

    string read_file( const fc::path& path )
    {
      std::ifstream s( path.string() );
      return string( ( std::istreambuf_iterator<char>( s ) ), std::istreambuf_iterator<char>() );
    }

    pqxx::result exec( const string& _query )
    {
      if( _query.size() > 100 )
        dlog("${q}..", ("q", _query.substr(0,100)) );
      else
        dlog("${q}", ("q", _query) );
      FC_ASSERT( trx );

      auto start = std::chrono::high_resolution_clock::now();

      pqxx::result res = trx->exec( _query );

      auto end = std::chrono::high_resolution_clock::now();
      dlog("query time[ms]: ${time}\n", ( "time", std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count() ));

      return res;
    }

    void finish_trx( bool force_rollback = false )
    {
      if( !trx )
        return;

      if( force_rollback || is_interrupted() )
      {
        dlog("Rollback..." );
        trx->rollback();
        dlog("Rollback finished..." );
      }
      else
      {
        dlog("Commit..." );
        trx->commit();
        dlog("Commit finished..." );
      }
      trx.reset();
    }

    template<typename T>
    bool get_single_value( const pqxx::result& res, T& _return ) const
    {
      if( res.size() == 1 )
      {
        auto _res = res.at(0);
        if( _res.size() == 1 )
        {
          _return = _res.at(0).as<T>();
          return true;
        }
      }
      return false;
    }

    bool context_exists()
    {
      if( is_interrupted() )
        return false;

      bool _val = false;

      pqxx::result _res = exec( query.check_context );
      get_single_value( _res, _val );

      return _val;
    }

    void execute_query( const std::vector< string >& queries, const ah_query::part_query_type& q_parts )
    {
      if( is_interrupted() )
        return;

      if( queries.empty() )
        return;

      uint64_t cnt = 0;
      string _total_query;

      for( auto& query : queries )
      {
        _total_query += ( cnt ? "," : "" ) + query;
        ++cnt;
      }

      _total_query = query.create_full_query( q_parts[0], _total_query, q_parts[2] );

      exec( _total_query );
    }

    ah_loader()
    : interrupted( false ), query( application_context )
    {
    }

  public:

    static ah_loader::ah_loader_ptr instance()
    {
      static ah_loader_ptr _obj = ah_loader_ptr( new ah_loader() );
      return _obj;
    }

    void init( const args_container& _args )
    {
      args = _args;
      tx_controller = hive::utilities::build_single_transaction_controller( args.url );
    }

    void interrupt()
    {
      if( !is_interrupted() )
        interrupted.store( true );
    }

    bool is_interrupted()
    {
      return interrupted.load();
    }

    void prepare()
    {
      if( is_interrupted() )
        return;

      try
      {
        FC_ASSERT( tx_controller );
        trx = tx_controller->openTx();

        if( !context_exists() )
        {
          string tables_query    = read_file( args.schema_path / "ah_schema_tables.sql" );
          string functions_query = read_file( args.schema_path / "ah_schema_functions.sql" );

          tables_query = query.format( tables_query, application_context, application_context, application_context, application_context );

          if( is_interrupted() )
            return;
          exec( query.create_context );

          if( is_interrupted() )
            return;
          exec( tables_query );

          if( is_interrupted() )
            return;
          exec( functions_query );
        }

        finish_trx();
      }
      catch(const pqxx::pqxx_exception& ex)
      {
        elog("AH loader detected SQL execution failure: ${e}", ("e", ex.base().what()));
        finish_trx( true/*force_rollback*/ );
      }
      catch(const fc::exception& ex)
      {
        elog("AH loader execution failed: ${e}", ("e", ex.what()));
        finish_trx( true/*force_rollback*/ );
      }
      catch(const std::exception& ex)
      {
        elog("AH loader execution failed: ${e}", ("e", ex.what()));
        finish_trx( true/*force_rollback*/ );
      }
      catch(...)
      {
        finish_trx( true/*force_rollback*/ );
      }
    }

    void process( uint64_t first_block, uint64_t last_block )
    {
      if( is_interrupted() )
        return;

      dlog("Processed blocks: ${fb} - ${lb}", ("fb", first_block)("lb", last_block) );

      string _query = query.format( query.query_executor, first_block, last_block );
      exec( _query );
    }

    bool process()
    {
      if( is_interrupted() )
        return true;

      try
      {
        uint64_t _first_block = 0;
        uint64_t _last_block  = 0;

        FC_ASSERT( tx_controller );

        trx = tx_controller->openTx();
        pqxx::result _range_blocks = exec( query.next_block );
        finish_trx();

        if( !_range_blocks.empty() )
        {
          FC_ASSERT( _range_blocks.size() == 1 );

          const auto& record = _range_blocks[0];

          if( record[0].is_null() || record[1].is_null() )
            return true;
          else
          {
            _first_block  = record[0].as<uint64_t>();
            _last_block   = record[1].as<uint64_t>();
          }

          trx = tx_controller->openTx();
          exec( query.detach_context );
          finish_trx();

          if( _last_block - _first_block > 0 )
          {
            uint64_t __last_block = _first_block;

            while( __last_block != _last_block )
            {
              trx = tx_controller->openTx();
              __last_block = std::min( __last_block + args.flush_size, _last_block );
              process( _first_block, __last_block );
              _first_block = __last_block + 1;
              finish_trx();
            }

            trx = tx_controller->openTx();
            auto _attach_context_query  = query.format( query.attach_context, application_context, __last_block );
            exec( _attach_context_query );
            finish_trx();
          }
          else
          {
            process( _first_block, _last_block );
          }
        }
        else
        {
          return true;
        }
      }
      catch(...)
      {
        finish_trx( true/*force_rollback*/ );
        return true;
      }

      return false;
    }

};

std::tuple<string, string, uint64_t, uint64_t > process_arguments( int argc, char** argv )
{
  // Setup logging config
  bpo::options_description opts;

  //--url postgresql://mario:mario@localhost/hivemind
  opts.add_options()("url", bpo::value<string>(), "postgres connection string for AH database");
  opts.add_options()("schema-dir", bpo::value<string>(), "directory where schemas are stored");
  opts.add_options()("range-blocks-flush", bpo::value<uint64_t>()->default_value( 1000 ), "Number of blocks processed at once");
  opts.add_options()("allowed-empty-results", bpo::value<int64_t>()->default_value( -1 ), "Allowed number of empty results from a database. After N tries, an application closes. A value `-1` means an infinite number of tries");

  bpo::variables_map options;

  bpo::store( bpo::parse_command_line(argc, argv, opts), options );

  FC_ASSERT( options.count("url"), "`url` is required" );
  FC_ASSERT( options.count("schema-dir"), "`schema-dir` is required" );
  FC_ASSERT( options.count("range-blocks-flush"), "`range-blocks-flush` is required" );
  FC_ASSERT( options.count("allowed-empty-results"), "`allowed-empty-results` is required" );

  return {  options["url"].as<string>(),
            options["schema-dir"].as<string>(),
            options["range-blocks-flush"].as<uint64_t>(),
            options["allowed-empty-results"].as<int64_t>() };
}

bool allow_close_app( bool empty, int64_t declared_empty_results, int64_t& cnt_empty_result )
{
  bool res = false;

  cnt_empty_result = empty ? ( cnt_empty_result + 1 ) : 0;

  if( declared_empty_results == -1 )
  {
    if( empty )
      dlog("A result returned from a database is empty. Actual empty result: ${er}", ("er", cnt_empty_result) );
  }
  else
  {
    if( empty )
    {
      dlog("A result returned from a database is empty. Declared empty results: ${der} Actual empty result: ${er}", ("der", declared_empty_results)("er", cnt_empty_result) );

      if( declared_empty_results < cnt_empty_result )
      {
        res = true;
      }
    }
  }

  return res;
}

void shutdown_properly()
{
  dlog("Closing. Wait...");

  auto _loader = ah_loader::instance();
  _loader->interrupt();

  dlog("Interrupted...");
}

void handle_signal_action( int signal )
{
  if( signal == SIGINT )
  {
    dlog("SIGINT was caught");
    shutdown_properly();
  }
  else if ( signal == SIGTERM )
  {
    dlog("SIGTERM was caught");
    shutdown_properly();
  }
  else
    dlog("Not supported signal ${signal}", ( "signal", signal ));
}

int setup_signals()
{
  struct sigaction sa;
  sa.sa_handler = handle_signal_action;
  sa.sa_flags = SA_RESTART;

  if( sigaction( SIGINT, &sa, 0 ) != 0 )
  {
    dlog("Error setting `SIGINT` handler");
    return -1;
  }
  if( sigaction( SIGTERM, &sa, 0 ) != 0 )
  {
    dlog("Error setting `SIGTERM` handler");
    return -1;
  }
  
  return 0;
}

int main( int argc, char** argv )
{
  try
  {
    auto args = process_arguments( argc, argv );

    auto _loader = ah_loader::instance();

    FC_ASSERT( _loader );

    _loader->init( args_container{ std::get<0>( args ), std::get<1>( args ), std::get<2>( args ) } );

    if( setup_signals() != 0 )
      exit( EXIT_FAILURE );

    _loader->prepare();

    int64_t cnt_empty_result = 0;
    int64_t declared_empty_results = std::get<3>( args );

    auto total_start = std::chrono::high_resolution_clock::now();

    while( !_loader->is_interrupted() )
    {
      auto start = std::chrono::high_resolution_clock::now();

      bool empty = _loader->process();

      auto end = std::chrono::high_resolution_clock::now();
      dlog("time[ms]: ${time}\n", ( "time", std::chrono::duration_cast< std::chrono::milliseconds >( end - start ).count() ));
 
      if( allow_close_app( empty, declared_empty_results, cnt_empty_result ) )
        break;
    }

    auto total_end = std::chrono::high_resolution_clock::now();
    dlog("*****Total time*****");
    dlog("total time[s]: ${time}", ( "time", std::chrono::duration_cast< std::chrono::milliseconds >( total_end - total_start ).count() / 1000 ));

    if( _loader->is_interrupted() )
      dlog("An application was interrupted...");

  }
  catch ( const boost::exception& e )
  {
    std::cerr << boost::diagnostic_information(e) << "\n";
    return EXIT_FAILURE;
  }
  catch ( const fc::exception& e )
  {
    std::cerr << e.to_detail_string() << "\n";
    return EXIT_FAILURE;
  }
  catch ( const std::exception& e )
  {
    std::cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }
  catch ( ... )
  {
    std::cerr << "unknown exception\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

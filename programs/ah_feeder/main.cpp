#include <hive/protocol/protocol.hpp>
#include <hive/protocol/types.hpp>

#include <hive/utilities/transaction_controllers.hpp>

#include <hive/chain/util/impacted.hpp>

#include <fc/string.hpp>
#include <fc/variant.hpp>
#include <fc/static_variant.hpp>
#include <fc/io/json.hpp>

#include <fstream>
#include <streambuf>
#include <chrono>

#include <boost/program_options.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/format.hpp>

#include <pqxx/pqxx>

namespace bpo = boost::program_options;

using hive::protocol::account_name_type;
using fc::string;

using hive::utilities::transaction_controller;
using hive::utilities::transaction_controller_ptr;

using transaction_ptr = transaction_controller::transaction_ptr;

struct account_info
{
  account_info( uint64_t _id, uint64_t _operation_count ) : id( _id ), operation_count( _operation_count ) {}

  static uint64_t next_account_id;

  uint64_t id;
  uint64_t operation_count;
};

uint64_t account_info::next_account_id = 0;

struct ah_query
{
  using part_query_type = std::array<string, 3>;

  const string application_context;

  string accounts;
  string account_ops;

  string create_context;
  string detach_context;
  string attach_context;
  string check_context;

  string next_block;

  string get_bodies;

  part_query_type insert_into_accounts;
  part_query_type insert_into_account_ops;

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

  string create_full_query( const string& q_parts0, const string& q_parts1, const string& q_parts2 )
  {
    return q_parts0 + q_parts1 + q_parts2;
  }

  ah_query( const string& _application_context ) : application_context( _application_context )
  {
    accounts        = "SELECT id, name FROM accounts;";
    account_ops     = "SELECT ai.name, ai.id, ai.operation_count FROM account_operation_count_info_view ai;";

    create_context  = format( "SELECT * FROM hive.app_create_context('%s');", application_context );
    detach_context  = format( "SELECT * FROM hive.app_context_detach('%s');", application_context );
    attach_context  = "SELECT * FROM hive.app_context_attach('%s', %d);";
    check_context   = format( "SELECT * FROM hive.app_context_exists('%s');", application_context );

    next_block      = format( "SELECT * FROM hive.app_next_block('%s');", application_context );

    get_bodies      = "SELECT id, body FROM hive.account_history_operations_view WHERE block_num >= %d AND block_num <= %d;";

    FC_ASSERT( insert_into_accounts.size() == 3 );
    insert_into_accounts[0] = " INSERT INTO public.accounts( id, name ) VALUES";
    insert_into_accounts[1] = " ( %d, '%s')";
    insert_into_accounts[2] = " ON CONFLICT DO NOTHING;";

    FC_ASSERT( insert_into_account_ops.size() == 3 );
    insert_into_account_ops[0] = " INSERT INTO public.account_operations( account_id, account_op_seq_no, operation_id ) VALUES";
    insert_into_account_ops[1] = " ( %d, %d, %d )";
    insert_into_account_ops[2] = " ON CONFLICT DO NOTHING;";
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
  private:

    using account_cache_t = std::map<string, account_info>;

    args_container args;

    const string application_context = "account_history";

    std::vector< string > accounts_queries;
    std::vector< string > account_ops_queries;

    account_cache_t account_cache;

    ah_query query;

    transaction_controller_ptr tx_controller;

    string read_file( const fc::path& path )
    {
      std::ifstream s( path.string() );
      return string( ( std::istreambuf_iterator<char>( s ) ), std::istreambuf_iterator<char>() );
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

    void import_accounts( transaction_ptr& trx )
    {
      pqxx::result _accounts = trx->exec( query.accounts );

      for( const auto& _record : _accounts )
      {
        uint64_t _id  = _record["id"].as<uint64_t>();
        string _name  = _record["name"].as<string>();

        if( _id > account_info::next_account_id )
          account_info::next_account_id = _id;

        account_cache.emplace( _name, account_info( _id, 0 ) );
      }

      if( account_info::next_account_id )
        ++account_info::next_account_id;
    }

    void import_account_operations( transaction_ptr& trx )
    {
      pqxx::result _account_ops = trx->exec( query.account_ops );

      for( const auto& _record : _account_ops )
      {
        string _name              = _record["name"].as<string>();
        uint64_t _operation_count = _record["operation_count"].as<uint64_t>();

        auto found = account_cache.find( _name );
        FC_ASSERT( found != account_cache.end() );

        found->second.operation_count = _operation_count;
      }
    }

    void import_initial_data( transaction_ptr& trx )
    {
      import_accounts( trx );
      import_account_operations( trx );
    }

    bool context_exists( transaction_ptr& trx )
    {
      bool _val = false;

      pqxx::result _res = trx->exec( query.check_context );
      get_single_value( _res, _val );

      return _val;
    }

    void gather_part_of_queries( transaction_ptr& trx, uint64_t operation_id, const string& account_name )
    {
      auto found = account_cache.find( account_name );

      auto _next_account_id = account_info::next_account_id;
      uint64_t _op_cnt = 0;

      if( found == account_cache.end() )
      {
        account_cache.emplace( account_name, account_info( _next_account_id, 1 ) );
        accounts_queries.emplace_back( query.format( query.insert_into_accounts[1], _next_account_id, account_name ) );

        ++account_info::next_account_id;
      }
      else
      {
        ++( found->second.operation_count );
        _op_cnt = found->second.operation_count;
      }

      account_ops_queries.emplace_back( query.format( query.insert_into_account_ops[1], _next_account_id, _op_cnt, operation_id ) );
    }

    void execute_query( transaction_ptr& trx, const std::vector< string >& queries, const ah_query::part_query_type& q_parts )
    {
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

      trx->exec( _total_query );
    }

    void execute_queries( transaction_ptr& trx )
    {
      execute_query( trx, accounts_queries, query.insert_into_accounts );
      execute_query( trx, account_ops_queries, query.insert_into_account_ops );

      accounts_queries.clear();
      account_ops_queries.clear();
    }

  public:

    ah_loader( const args_container& _args )
    : args{ _args }, query( application_context )
    {
      tx_controller = hive::utilities::build_single_transaction_controller( args.url );
    }

    void prepare()
    {
      transaction_ptr trx = tx_controller->openTx();

      if( !context_exists( trx ) )
      {
        string tables_query    = read_file( args.schema_path / "ah_schema_tables.sql" );
        string functions_query = read_file( args.schema_path / "ah_schema_functions.sql" );

        tables_query = query.format( tables_query, application_context, application_context );

        trx->exec( query.create_context );
        trx->exec( tables_query );
        trx->exec( functions_query );
      }

      import_initial_data( trx );

      trx->commit();
    }

    bool process( transaction_ptr& trx, uint64_t first_block, uint64_t last_block )
    {
      ilog("Processed blocks: ${fb} - ${lb}", ("fb", first_block)("lb", last_block) );

      trx->exec( query.detach_context );

      string _query = query.format( query.get_bodies, first_block, last_block );
      pqxx::result _blocks_ops = trx->exec( _query );

      if( !_blocks_ops.empty() )
      {
        for( const auto& _record : _blocks_ops )
        {
          uint64_t _operation_id  = _record[0].as<uint64_t>();
          string _op_body         = _record[1].as<string>();

          hive::protocol::operation _op;
          from_variant( fc::json::from_string( _op_body ), _op );

          flat_set<account_name_type> impacted;
          hive::app::operation_get_impacted_accounts( _op, impacted );

          string n;
          _op.visit( fc::get_static_variant_name( n ) );

          for( auto& account_name : impacted )
          {
            gather_part_of_queries( trx, _operation_id, account_name );
          }
        }
        execute_queries( trx );
      }

      auto _attach_context_query  = query.format( query.attach_context, application_context, last_block );
      trx->exec( _attach_context_query );

      return true;
    }

    bool process()
    {
      bool empty = false;

      uint64_t _first_block = 0;
      uint64_t _last_block  = 0;

      transaction_ptr trx = tx_controller->openTx();

      pqxx::result _range_blocks = trx->exec( query.next_block );

      if( !_range_blocks.empty() )
      {
          FC_ASSERT( _range_blocks.size() == 1 );

          const auto& record = _range_blocks[0];

          try
          {
            _first_block = record[0].as<uint64_t>();
            _last_block = record[1].as<uint64_t>();
          }
          catch(...)
          {
            trx->rollback();
            return true;
          }

          try
          {
            if( _last_block - _first_block > 0 )
            {
              if( ( _last_block - _first_block + 1 ) > args.flush_size )
                process( trx, _first_block, _first_block + args.flush_size - 1 );
              else
                process( trx, _first_block, _last_block );
            }
            else
            {
              process( trx, _first_block, _last_block );
            }
          }
          catch(...)
          {
            trx->rollback();
            return true;
          }
      }
      else
      {
        empty = true;
      }

      trx->commit();

      return empty;
    }

};

std::tuple<string, string, uint64_t, uint64_t > process_arguments( int argc, char** argv )
{
  // Setup logging config
  bpo::options_description opts;

  //ahsql-url = "dbname=block_mario user=mario password=mario hostaddr=127.0.0.1 port=5432"
  //postgresql://mario:mario@localhost/hivemind
  opts.add_options()("ahsql-url", bpo::value<string>(), "postgres connection string for AH database");
  opts.add_options()("ahsql-schema-dir", bpo::value<string>(), "directory where schemas are stored");
  opts.add_options()("ahsql-flush-size", bpo::value<uint64_t>()->default_value( 1000 ), "Number of blocks processed at once");
  opts.add_options()("ahsql-empty-results", bpo::value<uint64_t>()->default_value( 100 ), "Number of results from a database when an empty set");

  bpo::variables_map options;

  bpo::store( bpo::parse_command_line(argc, argv, opts), options );

  FC_ASSERT( options.count("ahsql-url"), "`ahsql-url` is required" );
  FC_ASSERT( options.count("ahsql-schema-dir"), "`ahsql-schema-dir` is required" );
  FC_ASSERT( options.count("ahsql-flush-size"), "`ahsql-flush-size` is required" );
  FC_ASSERT( options.count("ahsql-empty-results"), "`ahsql-empty-results` is required" );

  return {  options["ahsql-url"].as<string>(),
            options["ahsql-schema-dir"].as<string>(),
            options["ahsql-flush-size"].as<uint64_t>(),
            options["ahsql-empty-results"].as<uint64_t>() };
}

int main( int argc, char** argv )
{
  try
  {
    auto args = process_arguments( argc, argv );

    ah_loader loader( args_container{ std::get<0>( args ), std::get<1>( args ), std::get<2>( args ) } );

    loader.prepare();

    uint64_t cnt_empty_result = 0;
    uint64_t declared_empty_results = std::get<3>( args );

    while( true )
    {
      auto start = std::chrono::high_resolution_clock::now();

      bool empty = loader.process();

      auto end = std::chrono::high_resolution_clock::now();
      auto milliseconds = std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count();
      ilog("time[ms]: ${m}", ("m", milliseconds));
      ilog("***");

      if( empty )
      {
        ilog("result returned from a database is empty. Declared empty results: ${der} Actual empty result: ${er}", ("der", declared_empty_results)("er", cnt_empty_result) );

        ++cnt_empty_result;
        if( cnt_empty_result >= declared_empty_results )
        {
          return true;
        }
      }
      else
        cnt_empty_result = 0;
    }

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

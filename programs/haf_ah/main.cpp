#include <hive/protocol/protocol.hpp>
#include <hive/protocol/types.hpp>

#include <hive/sql_utilities/transaction_controllers.hpp>

#include <fc/string.hpp>

#include <fstream>
#include <streambuf>
#include <chrono>
#include <csignal>
#include <atomic>

#include <queue>
#include <mutex>
#include <condition_variable>

#include <thread>
#include <future>

#include <boost/program_options.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/format.hpp>

#include <pqxx/pqxx>

namespace bpo = boost::program_options;

using fc::string;

using hive::utilities::transaction_controller;
using hive::utilities::transaction_controller_ptr;

using transaction_ptr = transaction_controller::transaction_ptr;

struct range
{
  uint64_t low  = 0;
  uint64_t high = 0;
};

using ranges_type       = std::vector<range>;
using block_ranges_type = std::queue<range>;

struct account_op
{
  uint64_t op_id = 0;
  string name;
};
using account_ops_type = std::list< account_op >;
using received_items_type = std::list<account_ops_type>;

struct account_info
{
  account_info( uint64_t _id, uint64_t _operation_count ) : id( _id ), operation_count( _operation_count ) {}

  static uint64_t next_account_id;

  uint64_t id;
  uint64_t operation_count;
};

uint64_t account_info::next_account_id = 1;

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

  ah_query( const string& _application_context ) : application_context( _application_context )
  {
    accounts        = "SELECT id, name FROM accounts;";
    account_ops     = "SELECT ai.name, ai.id, ai.operation_count FROM account_operation_count_info_view ai;";

    create_context  = format( "SELECT * FROM hive.app_create_context('%s');", application_context );
    detach_context  = format( "SELECT * FROM hive.app_context_detach('%s');", application_context );
    attach_context  = "SELECT * FROM hive.app_context_attach('%s', %d);";
    check_context   = format( "SELECT * FROM hive.app_context_exists('%s');", application_context );

    next_block      = format( "SELECT * FROM hive.app_next_block('%s');", application_context );

    get_bodies      = "SELECT id, get_impacted_accounts(body) FROM hive.account_history_operations_view WHERE block_num >= %d AND block_num <= %d;";

    FC_ASSERT( insert_into_accounts.size() == 3 );
    insert_into_accounts[0] = "INSERT INTO public.accounts( id, name ) VALUES";
    insert_into_accounts[1] = " ( %d, '%s')";
    insert_into_accounts[2] = " ;";

    FC_ASSERT( insert_into_account_ops.size() == 3 );
    insert_into_account_ops[0] = "INSERT INTO public.account_operations( account_id, account_op_seq_no, operation_id ) VALUES";
    insert_into_account_ops[1] = " ( %d, %d, %d )";
    insert_into_account_ops[2] = " ;";
  }
};

struct args_container
{
  string    url;
  fc::path  schema_path;
  uint64_t  flush_size          = 0;
  uint32_t  nr_threads_receive  = 0;
  uint32_t  nr_threads_send     = 0;
};

template<typename T>
class thread_queue
{
  private:

    bool finished = false;

    std::mutex mx;
    std::condition_variable cv;

    std::queue<T> data;

  public:

  void set_finished()
  {
    finished = true;
  }

  bool is_finished()
  {
    return finished && data.empty();
  }

  void emplace( T&& obj )
  {
    std::lock_guard<std::mutex> lock( mx );

    data.emplace( obj );

    cv.notify_one();
  }

  T get()
  {
    std::unique_lock<std::mutex> lock( mx );

    while( data.empty() )
    {
      cv.wait( lock );
    }

    T val = data.front();
    data.pop();

    return val;
  }

};

class ah_loader
{
  public:

    using ah_loader_ptr = std::shared_ptr<ah_loader>;

  private:

    using account_cache_t = std::map<string, account_info>;

    std::atomic_bool interrupted;

    args_container args;

    const string application_context = "account_history";

    std::vector< string > accounts_queries;
    std::vector< string > account_ops_queries;

    thread_queue<received_items_type> queue;

    account_cache_t account_cache;

    ah_query query;

    std::vector< transaction_controller_ptr > tx_controllers;

    block_ranges_type block_ranges;

    string read_file( const fc::path& path )
    {
      std::ifstream s( path.string() );
      return string( ( std::istreambuf_iterator<char>( s ) ), std::istreambuf_iterator<char>() );
    }

    pqxx::result exec( transaction_ptr& trx, const string& _query )
    {
      if( _query.size() > 100 )
        dlog("${q}..", ("q", _query.substr(0,100)) );
      else
        dlog("${q}", ("q", _query) );
      FC_ASSERT( trx );

      auto start = std::chrono::high_resolution_clock::now();

      pqxx::result res = trx->exec( _query );

      auto end = std::chrono::high_resolution_clock::now();
      dlog("query time[ms]: ${time}", ( "time", std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count() ));

      return res;
    }

    void finish_trx( transaction_ptr& trx, bool force_rollback = false )
    {
      if( !trx )
        return;

      if( force_rollback || is_interrupted() )
      {
        dlog("Rollback..." );
        trx->rollback();
        dlog("Rollback finished...\n" );
      }
      else
      {
        dlog("Commit..." );
        trx->commit();
        dlog("Commit finished...\n" );
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

    void import_accounts( transaction_ptr& trx )
    {
      if( is_interrupted() )
        return;

      pqxx::result _accounts = exec( trx, query.accounts );

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
      if( is_interrupted() )
        return;

      pqxx::result _account_ops = exec( trx, query.account_ops );

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
      if( is_interrupted() )
        return false;

      bool _val = false;

      pqxx::result _res = exec( trx, query.check_context );
      get_single_value( _res, _val );

      return _val;
    }

    void gather_part_of_queries( uint64_t operation_id, const string& account_name )
    {
      auto found = account_cache.find( account_name );

      auto _next_account_id = account_info::next_account_id;
      uint64_t _op_cnt = 1;

      if( found == account_cache.end() )
      {
        account_cache.emplace( account_name, account_info( _next_account_id, _op_cnt ) );
        accounts_queries.emplace_back( query.format( query.insert_into_accounts[1], _next_account_id, account_name ) );

        ++account_info::next_account_id;
      }
      else
      {
        _next_account_id  = found->second.id;
        ++( found->second.operation_count );
        _op_cnt           = found->second.operation_count;
      }

      account_ops_queries.emplace_back( query.format( query.insert_into_account_ops[1], _next_account_id, _op_cnt, operation_id ) );
    }

    void execute_query( transaction_ptr& trx, const std::vector< string >& queries, uint32_t low, uint32_t high, const ah_query::part_query_type& q_parts )
    {
      if( is_interrupted() )
        return;

      if( queries.empty() )
        return;

      uint64_t cnt = 0;
      std::stringstream _total_query;
      _total_query << q_parts[0];

      for( uint32_t i = low; i <= high; ++i )
      {
        _total_query << ( cnt ? "," : "" ) + queries[i];
        ++cnt;
      }

      _total_query << q_parts[2];

      exec( trx, _total_query.str() );
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

      //Here is `+2` because for sending it's necessary to have an additional connections.
      for( uint32_t i = 0; i < args.nr_threads_receive + 2; ++i )
      {
        tx_controllers.emplace_back( hive::utilities::build_own_transaction_controller( args.url ) );
      }
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

      FC_ASSERT( tx_controllers.size() );
      transaction_ptr trx = tx_controllers[ tx_controllers.size() - 1 ]->openTx();

      try
      {
        if( !context_exists( trx ) )
        {
          string tables_query    = read_file( args.schema_path / "ah_schema_tables.sql" );
          string functions_query = read_file( args.schema_path / "ah_schema_functions.sql" );

          if( is_interrupted() )
            return;
          exec( trx, query.create_context );

          if( is_interrupted() )
            return;
          exec( trx, tables_query );

          if( is_interrupted() )
            return;
          exec( trx, functions_query );
        }

        import_initial_data( trx );

        finish_trx( trx );
      }
      catch(const pqxx::pqxx_exception& ex)
      {
        elog("Error: ${e}", ("e", ex.base().what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(const fc::exception& ex)
      {
        elog("Error: ${e}", ("e", ex.what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(const std::exception& ex)
      {
        elog("Error: ${e}", ("e", ex.what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(...)
      {
        finish_trx( trx, true/*force_rollback*/ );
      }
    }

    ranges_type prepare_ranges( uint64_t low_value, uint64_t high_value, uint32_t threads )
    {
      FC_ASSERT( threads > 0 && threads <= 64 );

      if( threads == 1 )
      {
        return { range{ low_value, high_value } };
      }

      std::vector<range> ranges{ threads };
      uint64_t _size = ( high_value - low_value + 1 ) / threads;

      for( size_t i = 0; i < ranges.size(); ++i )
      {
        if( i == 0 )
        {
          ranges[i].low   = low_value;
          ranges[i].high  = low_value + _size;
        }
        else
        {
          ranges[i].low   = ranges[i - 1].high + 1;
          ranges[i].high  = ranges[i].low + _size;
        }
      }
      ranges[ranges.size() - 1].high = high_value;

      return ranges;
    }

    account_ops_type worker_receive_internal( uint32_t index, uint64_t first_block, uint64_t last_block )
    {
      account_ops_type _items;

      FC_ASSERT( index < tx_controllers.size() );
      transaction_ptr trx = tx_controllers[ index ]->openTx();

      try
      {
        dlog("Receiving impacted accounts: from ${f} block to ${l} block", ("f", first_block)("l", last_block) );

        string _query = query.format( query.get_bodies, first_block, last_block );
        pqxx::result _result = exec( trx, _query );

        if( !_result.empty() )
        {
          dlog("Found ${s} operations", ("s", _result.size()) );
          for( const auto& _record : _result )
          {
            _items.emplace_back( account_op{ _record[0].as<uint64_t>(), _record[1].as<string>() } );
          }
        }

        finish_trx( trx );
      }
      catch(const pqxx::pqxx_exception& ex)
      {
        elog("Error: ${e}", ("e", ex.base().what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(const fc::exception& ex)
      {
        elog("Error: ${e}", ("e", ex.what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(const std::exception& ex)
      {
        elog("Error: ${e}", ("e", ex.what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(...)
      {
        finish_trx( trx, true/*force_rollback*/ );
      }

      return _items;
    }

    void worker_receive( uint32_t index, uint64_t first_block, uint64_t last_block, std::promise<account_ops_type> result_promise )
    {
      result_promise.set_value( std::move( worker_receive_internal( index, first_block, last_block ) ) );
    }

    void receive_internal( uint64_t first_block, uint64_t last_block )
    {
      auto start = std::chrono::high_resolution_clock::now();

      ranges_type _ranges = prepare_ranges( first_block, last_block, args.nr_threads_receive );

      using promise_type = std::promise<account_ops_type>;
      using future_type = std::future<account_ops_type>;

      std::list<std::thread> threads_receive;
      std::list<promise_type> promises{ _ranges.size() };
      std::list<future_type> futures;
      uint32_t _idx = 0;
      for( auto& promise : promises )
      {
        futures.emplace_back( promise.get_future() );
        threads_receive.emplace_back( std::move( std::thread( &ah_loader::worker_receive, this, _idx, _ranges[_idx].low, _ranges[_idx].high, std::move( promise ) ) ) );
        ++_idx;
      }

      auto end = std::chrono::high_resolution_clock::now();
      dlog("prepare-receive time[ms]: ${time}", ( "time", std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count() ));
      start = std::chrono::high_resolution_clock::now();

      received_items_type _buffer;
      for( auto& future : futures )
      {
        _buffer.emplace_back( future.get() );
      }

      end = std::chrono::high_resolution_clock::now();
      dlog("buffer-receive time[ms]: ${time}", ( "time", std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count() ));
      start = std::chrono::high_resolution_clock::now();

      queue.emplace( std::move( _buffer ) );

      end = std::chrono::high_resolution_clock::now();
      dlog("queue-receive time[ms]: ${time}", ( "time", std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count() ));
      start = std::chrono::high_resolution_clock::now();

      for( auto& thread : threads_receive )
        thread.join();

      end = std::chrono::high_resolution_clock::now();
      dlog("join-receive time[ms]: ${time}", ( "time", std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count() ));
    }

    void receive()
    {
      while( !block_ranges.empty() )
      {
        auto start = std::chrono::high_resolution_clock::now();

        auto _item = block_ranges.front();
        block_ranges.pop();

        receive_internal( _item.low, _item.high );

        auto end = std::chrono::high_resolution_clock::now();
        dlog("receive time[ms]: ${time}", ( "time", std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count() ));
      }

      queue.set_finished();
    }

    void prepare_sql()
    {
      auto received_items = queue.get();

      for( auto& items : received_items )
      {
        for( auto& item : items )
          gather_part_of_queries( item.op_id, item.name );
      }
    }

    void send_accounts( uint32_t index )
    {
      FC_ASSERT( index < tx_controllers.size() );
      transaction_ptr trx = tx_controllers[ index ]->openTx();

      execute_query( trx, accounts_queries, 0, accounts_queries.size() - 1, query.insert_into_accounts );
      accounts_queries.clear();
      finish_trx( trx );
    }

    void send_internal( uint32_t index, uint64_t first_element, uint64_t last_element )
    {
      FC_ASSERT( index < tx_controllers.size() );
      transaction_ptr trx = tx_controllers[ index ]->openTx();

      try
      {
        dlog("INSERT INTO to `account_operations`: first element: ${f} last element: ${l}", ("f", first_element)("l", last_element) );

        execute_query( trx, account_ops_queries, first_element, last_element, query.insert_into_account_ops );

        finish_trx( trx );
      }
      catch(const pqxx::pqxx_exception& ex)
      {
        elog("Error: ${e}", ("e", ex.base().what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(const fc::exception& ex)
      {
        elog("Error: ${e}", ("e", ex.what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(const std::exception& ex)
      {
        elog("Error: ${e}", ("e", ex.what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(...)
      {
        finish_trx( trx, true/*force_rollback*/ );
      }
    }

    void send_account_operations( uint32_t index )
    {
      send_internal( index, 0, account_ops_queries.size() - 1 );
      account_ops_queries.clear();
    }

    void send()
    {
      while( !queue.is_finished() )
      {
        auto start = std::chrono::high_resolution_clock::now();

        if( is_interrupted() )
          return;

        prepare_sql();

        if( is_interrupted() )
          return;

        FC_ASSERT( tx_controllers.size() > 2 );

        std::thread thread_accounts( &ah_loader::send_accounts, this, tx_controllers.size() - 1 );
        std::thread thread_account_operations( &ah_loader::send_account_operations, this, tx_controllers.size() - 2 );

        thread_accounts.join();
        thread_account_operations.join();

        auto end = std::chrono::high_resolution_clock::now();
        dlog("send time[ms]: ${time}", ( "time", std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count() ));
      }
    }

    void work()
    {
      if( is_interrupted() )
        return;

      std::thread thread_receive( &ah_loader::receive, this );
      std::thread thread_send( &ah_loader::send, this );

      thread_receive.join();
      thread_send.join();
    }

    void fill_block_ranges( uint64_t first_block, uint64_t last_block )
    {
      uint64_t _last_block = first_block;

      while( _last_block != last_block )
      {
        _last_block = std::min( _last_block + args.flush_size, last_block );
        block_ranges.emplace( range{ first_block, _last_block } );
        first_block = _last_block + 1;
      }
    }

    bool process()
    {
      if( is_interrupted() )
        return true;

      transaction_ptr trx;
      try
      {
        uint64_t _first_block = 0;
        uint64_t _last_block  = 0;


        FC_ASSERT( tx_controllers.size() );
        transaction_controller_ptr tx_controller = tx_controllers[ tx_controllers.size() - 1 ];
      
        trx = tx_controller->openTx();

        pqxx::result _range_blocks = exec( trx, query.next_block );
        finish_trx( trx );

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
          exec( trx, query.detach_context );
          finish_trx( trx );

          fill_block_ranges( _first_block, _last_block );
          work();

          if( _last_block - _first_block > 0 )
          {
            trx = tx_controller->openTx();
            auto _attach_context_query  = query.format( query.attach_context, application_context, _last_block );
            exec( trx, _attach_context_query );
            finish_trx( trx );
          }
        }
        else
        {
          return true;
        }
      }
      catch(const pqxx::pqxx_exception& ex)
      {
        elog("Error: ${e}", ("e", ex.base().what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(const fc::exception& ex)
      {
        elog("Error: ${e}", ("e", ex.what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(const std::exception& ex)
      {
        elog("Error: ${e}", ("e", ex.what()));
        finish_trx( trx, true/*force_rollback*/ );
      }
      catch(...)
      {
        finish_trx( trx, true/*force_rollback*/ );
      }

      return false;
    }

};

std::tuple<string, string, uint64_t, uint64_t, uint32_t, uint32_t > process_arguments( int argc, char** argv )
{
  // Setup logging config
  bpo::options_description opts;

  //--url postgresql://mario:mario@localhost/hivemind
  opts.add_options()("url", bpo::value<string>(), "postgres connection string for AH database");
  opts.add_options()("schema-dir", bpo::value<string>(), "directory where schemas are stored");
  opts.add_options()("range-blocks-flush", bpo::value<uint64_t>()->default_value( 1000 ), "Number of blocks processed at once");
  opts.add_options()("allowed-empty-results", bpo::value<int64_t>()->default_value( -1 ), "Allowed number of empty results from a database. After N tries, an application closes. A value `-1` means an infinite number of tries");
  opts.add_options()("threads_receive", bpo::value<uint32_t>()->default_value( 1 ), "Number of threads that are used during retrieving `get_impacted_accounts` data");
  opts.add_options()("threads_send", bpo::value<uint32_t>()->default_value( 1 ), "Number of threads that are used during sending data into database");

  bpo::variables_map options;

  bpo::store( bpo::parse_command_line(argc, argv, opts), options );

  FC_ASSERT( options.count("url"), "`url` is required" );
  FC_ASSERT( options.count("schema-dir"), "`schema-dir` is required" );
  FC_ASSERT( options.count("range-blocks-flush"), "`range-blocks-flush` is required" );
  FC_ASSERT( options.count("threads_receive"), "`threads_receive` is required" );
  FC_ASSERT( options.count("threads_send"), "`threads_send` is required" );

  return {  options["url"].as<string>(),
            options["schema-dir"].as<string>(),
            options["range-blocks-flush"].as<uint64_t>(),
            options["allowed-empty-results"].as<int64_t>(),
            options["threads_receive"].as<uint32_t>(),
            options["threads_send"].as<uint32_t>() };
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

    _loader->init( args_container{ std::get<0>( args ), std::get<1>( args ), std::get<2>( args ), std::get<4>( args ), std::get<5>( args ) } );

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

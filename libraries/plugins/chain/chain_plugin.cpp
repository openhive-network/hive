#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <hive/plugins/chain/abstract_block_producer.hpp>
#include <hive/plugins/chain/state_snapshot_provider.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/statsd/utility.hpp>

#include <hive/utilities/key_conversion.hpp>
#include <hive/utilities/notifications.hpp>
#include <hive/utilities/benchmark_dumper.hpp>
#include <hive/utilities/database_configuration.hpp>

#include <fc/string.hpp>
#include <fc/io/json.hpp>
#include <fc/io/fstream.hpp>

#include <boost/asio.hpp>
#include <boost/optional.hpp>
//#include <boost/bind/bind.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/scope_exit.hpp>
#include <boost/thread/future.hpp>

#include <thread>
#include <chrono>
#include <memory>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace hive { namespace plugins { namespace chain {

#define BENCHMARK_FILE_NAME "replay_benchmark.json"

using namespace hive;

using fc::flat_map;
using hive::chain::block_id_type;

using hive::plugins::chain::synchronization_type;
using index_memory_details_cntr_t = hive::utilities::benchmark_dumper::index_memory_details_cntr_t;
using get_indexes_memory_details_type = std::function< void( index_memory_details_cntr_t&, bool ) >;

#define NUM_THREADS 1

typedef fc::static_variant<std::shared_ptr<boost::promise<void>>, fc::promise<void>::ptr> promise_ptr;

class transaction_flow_control
{
public:
  explicit transaction_flow_control( const std::shared_ptr<full_transaction_type>& _tx ) : tx( _tx ) {}

  void attach_promise( promise_ptr _p ) { prom_ptr = _p; }

  void on_failure( const fc::exception& ex ) { except = ex.dynamic_copy_exception(); }
  void on_worker_done();

  const std::shared_ptr<full_transaction_type>& get_full_transaction() const { return tx; }
  const fc::exception_ptr& get_exception() const { return except; }
  void rethrow_if_exception() const { if( except ) except->dynamic_rethrow_exception(); }

private:
  std::shared_ptr<full_transaction_type> tx;
  promise_ptr                            prom_ptr;
  fc::exception_ptr                      except;

  struct request_promise_visitor;
};

struct transaction_flow_control::request_promise_visitor
{
  request_promise_visitor() {}

  typedef void result_type;

  // n.b. our visitor functions take a shared pointer to the promise by value
  // so the promise can't be garbage collected until the set_value() function
  // has finished exiting.  There's a race where the calling thread wakes up
  // during the set_value() call and can delete the promise out from under us
  // unless we hold a reference here.
  void operator()( fc::promise<void>::ptr t )
  {
    t->set_value();
  }

  void operator()( std::shared_ptr<boost::promise<void>> t )
  {
    t->set_value();
  }
};

void transaction_flow_control::on_worker_done()
{
  request_promise_visitor prom_visitor;
  prom_ptr.visit( prom_visitor );
}

typedef fc::static_variant<
  std::shared_ptr< p2p_block_flow_control >,
  transaction_flow_control*,
  std::shared_ptr< generate_block_flow_control >
> write_request_ptr;

struct write_context
{
  write_request_ptr             req_ptr;
};

namespace detail {

class chain_plugin_impl
{
  public:
    chain_plugin_impl() {}
    ~chain_plugin_impl() 
    {
      stop_write_processing();
      if(dumper_post_apply_block.connected()) dumper_post_apply_block.disconnect();
    }

    void register_snapshot_provider(state_snapshot_provider& provider)
    {
      snapshot_provider = &provider;
    }

    void start_write_processing();
    void stop_write_processing();

    bool start_replay_processing();

    void initial_settings();
    void open();
    bool replay_blockchain();
    void process_snapshot();
    bool check_data_consistency();

    void work( synchronization_type& on_sync );

    void write_default_database_config( bfs::path& p );
    void setup_benchmark_dumper();

    uint64_t                         shared_memory_size = 0;
    uint16_t                         shared_file_full_threshold = 0;
    uint16_t                         shared_file_scale_rate = 0;
    uint32_t                         chainbase_flags = 0;
    bfs::path                        shared_memory_dir;
    bool                             replay = false;
    bool                             resync   = false;
    bool                             readonly = false;
    bool                             check_locks = false;
    bool                             validate_invariants = false;
    bool                             dump_memory_details = false;
    bool                             benchmark_is_enabled = false;
    bool                             statsd_on_replay = false;
    uint32_t                         stop_replay_at = 0;
    bool                             exit_after_replay = false;
    bool                             exit_before_sync = false;
    bool                             force_replay = false;
    bool                             validate_during_replay = false;
    uint32_t                         benchmark_interval = 0;
    uint32_t                         flush_interval = 0;
    bool                             replay_in_memory = false;
    std::vector< std::string >       replay_memory_indices{};
    bool                             enable_block_log_compression = true;
    int                              block_log_compression_level = 15;
    flat_map<uint32_t,block_id_type> loaded_checkpoints;

    uint32_t allow_future_time = 5;

    std::shared_ptr< std::thread >   write_processor_thread;

    // `writequeue` and `running` are guarded by the queue_mutex
    std::mutex                       queue_mutex;
    std::condition_variable          queue_condition_variable;
    std::queue<write_context*>       write_queue;
    bool                             running = true;

    int16_t                          write_lock_hold_time = HIVE_BLOCK_INTERVAL * 1000 / 6; // 1/6 of block time (millseconds)

    vector< string >                 loaded_plugins;
    fc::mutable_variant_object       plugin_state_opts;
    bfs::path                        database_cfg;

    database  db;
    std::string block_generator_registrant;
    std::shared_ptr< abstract_block_producer > block_generator;

    hive::utilities::benchmark_dumper   dumper;
    hive::chain::open_args              db_open_args;
    get_indexes_memory_details_type     get_indexes_memory_details;
    boost::signals2::connection         dumper_post_apply_block;

    state_snapshot_provider*            snapshot_provider = nullptr;
    bool                                is_p2p_enabled = true;
    std::atomic<uint32_t>               peer_count = {0};

    fc::time_point cumulative_times_last_reported_time;
    fc::microseconds cumulative_time_waiting_for_locks;
    fc::microseconds cumulative_time_processing_blocks;
    fc::microseconds cumulative_time_processing_transactions;
    fc::microseconds cumulative_time_waiting_for_work;

    fc::optional<fc::time_point> last_myriad_time; // for sync progress

    class write_request_visitor;
};

struct chain_plugin_impl::write_request_visitor
{
  write_request_visitor( chain_plugin_impl& _chain_plugin ) : cp( _chain_plugin ) {}

  //cumulative data on transactions since last block
  uint32_t         count_tx_pushed = 0;
  uint32_t         count_tx_applied = 0;
  uint32_t         count_tx_failed_auth = 0;
  uint32_t         count_tx_failed_no_rc = 0;

  chain_plugin_impl& cp;
  write_context* cxt = nullptr;

  typedef void result_type;

  void on_block( const block_flow_control* block_ctrl )
  {
    block_ctrl->on_write_queue_pop( count_tx_pushed, count_tx_applied, count_tx_failed_auth, count_tx_failed_no_rc );
    count_tx_pushed = 0;
    count_tx_applied = 0;
    count_tx_failed_auth = 0;
    count_tx_failed_no_rc = 0;
  }

  void operator()( std::shared_ptr< p2p_block_flow_control > p2p_block_ctrl )
  {
    try
    {
      STATSD_START_TIMER("chain", "write_time", "push_block", 1.0f)
      on_block( p2p_block_ctrl.get() );
      fc::time_point time_before_pushing_block = fc::time_point::now();
      BOOST_SCOPE_EXIT(this_, time_before_pushing_block, &statsd_timer) { 
        this_->cp.cumulative_time_processing_blocks += fc::time_point::now() - time_before_pushing_block;
        STATSD_STOP_TIMER("chain", "write_time", "push_block")
      } BOOST_SCOPE_EXIT_END
      cp.db.push_block( *p2p_block_ctrl.get(), p2p_block_ctrl->get_skip_flags() );
    }
    catch( const fc::exception& e )
    {
      p2p_block_ctrl->on_failure( e );
    }
    catch( ... )
    {
      p2p_block_ctrl->on_failure( fc::unhandled_exception( FC_LOG_MESSAGE( warn,
        "Unexpected exception while pushing block." ), std::current_exception() ) );
    }
    p2p_block_ctrl->on_worker_done();
  }

  void operator()( transaction_flow_control* tx_ctrl )
  {
    try
    {
      STATSD_START_TIMER( "chain", "write_time", "push_transaction", 1.0f )
      fc::time_point time_before_pushing_transaction = fc::time_point::now();
      ++count_tx_pushed;
      cp.db.push_transaction( tx_ctrl->get_full_transaction() );
      cp.cumulative_time_processing_transactions += fc::time_point::now() - time_before_pushing_transaction;
      STATSD_STOP_TIMER( "chain", "write_time", "push_transaction" )

      ++count_tx_applied;
    }
    catch( const hive::protocol::transaction_auth_exception& e )
    {
      tx_ctrl->on_failure( e );
      ++count_tx_failed_auth;
    }
    catch( const hive::chain::not_enough_rc_exception& e )
    {
      tx_ctrl->on_failure( e );
      ++count_tx_failed_no_rc;
    }
    catch( const fc::exception& e )
    {
      tx_ctrl->on_failure( e );
    }
    catch( ... )
    {
      elog("Unknown exception while pushing transaction.");
      tx_ctrl->on_failure( fc::unhandled_exception( FC_LOG_MESSAGE( warn,
        "Unexpected exception while pushing transaction." ), std::current_exception() ) );
    }
    tx_ctrl->on_worker_done();
  }

  void operator()( std::shared_ptr< generate_block_flow_control > generate_block_ctrl )
  {
    try
    {
      if( !cp.block_generator )
        FC_THROW_EXCEPTION( chain_exception, "Received a generate block request, but no block generator has been registered." );

      STATSD_START_TIMER( "chain", "write_time", "generate_block", 1.0f )
      on_block( generate_block_ctrl.get() );
      cp.block_generator->generate_block( generate_block_ctrl.get() );
      STATSD_STOP_TIMER( "chain", "write_time", "generate_block" )
    }
    catch( const fc::exception& e )
    {
      generate_block_ctrl->on_failure( e );
    }
    catch( ... )
    {
      generate_block_ctrl->on_failure( fc::unhandled_exception( FC_LOG_MESSAGE( warn,
        "Unexpected exception while generating block."), std::current_exception() ) );
    }
    generate_block_ctrl->on_worker_done();
  }
};

void chain_plugin_impl::start_write_processing()
{
  write_processor_thread = std::make_shared<std::thread>([&]()
  {
    try
    {
      hive::notify_hived_status("syncing");
      ilog("Write processing thread started.");
      fc::set_thread_name("write_queue");
      fc::thread::current().set_name("write_queue");
      cumulative_times_last_reported_time = fc::time_point::now();

      const fc::microseconds block_wait_max_time = fc::seconds(10 * HIVE_BLOCK_INTERVAL);
      bool is_syncing = true;
      write_request_visitor req_visitor( *this );

      /* This loop monitors the write request queue and performs writes to the database. These
        * can be blocks or pending transactions. Because the caller needs to know the success of
        * the write and any exceptions that are thrown, a write context is passed in the queue
        * to the processing thread which it will use to store the results of the write. It is the
        * caller's responsibility to ensure the pointer to the write context remains valid until
        * the contained promise is complete.
        *
        * The loop has two modes, sync mode and live mode. In sync mode we want to process writes
        * as quickly as possible with minimal overhead. The outer loop busy waits on the queue
        * and the inner loop drains the queue as quickly as possible. We exit sync mode when the
        * head block is within 1 minute of system time.
        *
        * Live mode needs to balance between processing pending writes and allowing readers access
        * to the database. It will batch writes together as much as possible to minimize lock
        * overhead but will willingly give up the write lock after 500ms. The thread then sleeps for
        * 10ms. This allows time for readers to access the database as well as more writes to come
        * in. When the node is live the rate at which writes come in is slower and busy waiting is
        * not an optimal use of system resources when we could give CPU time to read threads.
        */
      fc::time_point last_popped_item_time = fc::time_point::now();
      fc::time_point last_msg_time = last_popped_item_time;
      fc::time_point wait_start_time = last_popped_item_time;

      hive::notify_hived_status("syncing");
      while (true)
      {
        // print a message if we haven't gotten any new data in a while
        fc::time_point loop_start_time = fc::time_point::now();
        fc::microseconds time_since_last_popped_item = loop_start_time - last_popped_item_time;
        fc::microseconds time_since_last_message_printed = loop_start_time - last_msg_time;
        if (time_since_last_popped_item > block_wait_max_time && time_since_last_message_printed > block_wait_max_time)
        {
          wlog("No P2P data (block/transaction) received in last ${t} seconds... peer_count=${peer_count}", ("t", block_wait_max_time.to_seconds())("peer_count", peer_count.load()));
          last_msg_time = loop_start_time; /// To avoid log pollution
        }

        // try to dequeue a block/transaction
        write_context* cxt;
        {
          fc::microseconds max_time_to_wait = time_since_last_popped_item > block_wait_max_time ? block_wait_max_time - time_since_last_message_printed
                                                                                                : block_wait_max_time - time_since_last_popped_item;
          std::unique_lock<std::mutex> lock(queue_mutex);
          bool wait_timed_out = false;
          while (running && write_queue.empty() && !wait_timed_out)
            if (queue_condition_variable.wait_for(lock, std::chrono::microseconds(max_time_to_wait.count())) == std::cv_status::timeout)
              wait_timed_out = true;
          if (!running) // we woke because the node is shutting down
            break;
          if (wait_timed_out) // we timed out, restart the while loop to print a "No P2P data" message
            continue;
          // otherwise, we woke because the write_queue is non-empty
          cxt = write_queue.front();
          write_queue.pop();
        }

        cumulative_time_waiting_for_work += fc::time_point::now() - wait_start_time;
        last_popped_item_time = fc::time_point::now();

        fc::time_point write_lock_request_time = fc::time_point::now();
        fc::time_point_sec head_block_time;
        db.with_write_lock([&]()
        {
          uint32_t write_queue_items_processed = 0;
          fc::time_point write_lock_acquired_time = fc::time_point::now();
          fc::microseconds write_lock_acquisition_time = write_lock_acquired_time - write_lock_request_time;
          cumulative_time_waiting_for_locks += write_lock_acquisition_time;

          if( write_lock_acquisition_time > fc::milliseconds( 50 ) )
            wlog("write_lock_acquisition_time = ${write_lock_aquisition_time}μs exceeds warning threshold of 50ms",
                 ("write_lock_aquisition_time", write_lock_acquisition_time.count()));
          fc_dlog(fc::logger::get("chainlock"), "write_lock_acquisition_time = ${write_lock_aquisition_time}μs",
                 ("write_lock_aquisition_time", write_lock_acquisition_time.count()));
          STATSD_START_TIMER( "chain", "lock_time", "write_lock", 1.0f )
          while (true)
          {
            req_visitor.cxt = cxt;
            cxt->req_ptr.visit( req_visitor );

            ++write_queue_items_processed;

            if (!is_syncing) //if not syncing, we shouldn't take more than 500ms to process everything in the write queue
            {
              fc::microseconds write_lock_held_duration = fc::time_point::now() - write_lock_acquired_time;
              if (write_lock_held_duration > fc::milliseconds(write_lock_hold_time))
              {
                wlog("Stopped processing write_queue before empty because we exceeded ${write_lock_hold_time}ms, "
                     "held lock for ${write_lock_held_duration}μs",
                     (write_lock_hold_time)
                     ("write_lock_held_duration", write_lock_held_duration.count()));
                break;
              }
            }

            {
              std::unique_lock<std::mutex> lock(queue_mutex);
              if (!running || write_queue.empty())
              {
                fc::microseconds write_queue_processed_duration = fc::time_point::now() - write_lock_acquired_time;
                //if (write_queue_processed_duration > fc::milliseconds(500))
                  fc_wlog(fc::logger::get("chainlock"), "Emptied write_queue of ${write_queue_items_processed} items after ${write_queue_processed_duration}µs (${per_block}µs/block)",
                          (write_queue_items_processed)("write_queue_processed_duration", write_queue_processed_duration.count())
                          ("per_block", write_queue_processed_duration.count() / write_queue_items_processed));
                break;
              }
              cxt = write_queue.front();
              write_queue.pop();
            }

            last_popped_item_time = fc::time_point::now();
          } // while items in write_queue and time limit not exceeded for live sync
          head_block_time = db.head_block_time();
        }); // with_write_lock

        if (is_syncing && fc::time_point::now() - head_block_time < fc::minutes(1)) //we're syncing, see if we are close enough to move to live sync
        {
          is_syncing = false;
          db.notify_end_of_syncing();
          hive::notify_hived_status("entering live mode");
          wlog("entering live mode");
        }

        wait_start_time = fc::time_point::now();
        fc::microseconds time_since_last_report = fc::time_point::now() - cumulative_times_last_reported_time;
        if (time_since_last_report > fc::seconds(30))
        {
          fc::microseconds total_recorded_times = cumulative_time_waiting_for_locks + cumulative_time_processing_blocks + cumulative_time_processing_transactions + cumulative_time_waiting_for_work;
          float percent_waiting_for_locks = cumulative_time_waiting_for_locks.count() / (float)time_since_last_report.count() * 100.f;
          float percent_processing_blocks = cumulative_time_processing_blocks.count() / (float)time_since_last_report.count() * 100.f;
          float percent_processing_transactions = cumulative_time_processing_transactions.count() / (float)time_since_last_report.count() * 100.f;
          float percent_waiting_for_work = cumulative_time_waiting_for_work.count() / (float)time_since_last_report.count() * 100.f;
          float percent_unknown = (time_since_last_report - total_recorded_times).count() / (float)time_since_last_report.count() * 100.f;

          std::ostringstream report;
          report << std::setprecision(2) << std::fixed 
                 << "waiting for work: " << percent_waiting_for_work
                 << "%, waiting for locks: " << percent_waiting_for_locks 
                 << "%, processing transactions: " << percent_processing_transactions 
                 << "%, processing blocks: " << percent_processing_blocks 
                 << "%, unknown: " << percent_unknown << "%";
          wlog("${report}", ("report", report.str()));

          cumulative_time_waiting_for_locks = fc::microseconds();
          cumulative_time_processing_blocks = fc::microseconds();
          cumulative_time_processing_transactions = fc::microseconds();
          cumulative_time_waiting_for_work = fc::microseconds();
          cumulative_times_last_reported_time = fc::time_point::now();
        }
      } // while running
      ilog("Write processing thread finished.");
    }
    catch (const fc::exception& e)
    {
      elog("Unexpected exception is killing write_queue thread: ${e}", (e));
    }
    catch (const std::exception& e)
    {
      elog("Unexpected exception is killing write_queue thread: ${e}", ("e", e.what()));
    }
    catch (...)
    {
      elog("Unknown exception is killing write_queue thread.");
    }
  });
}

void chain_plugin_impl::stop_write_processing()
{
  hive::notify_hived_status("finished syncing");
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    running = false;
  }
  queue_condition_variable.notify_one();

  if( write_processor_thread )
  {
    ilog("Waiting for write processing thread finish...");
    write_processor_thread->join();
    ilog("Write processing thread stopped.");
  }

  write_processor_thread.reset();
}

bool chain_plugin_impl::start_replay_processing()
{
  hive::notify_hived_status("replaying");
  bool replay_is_last_operation = replay_blockchain();
  hive::notify_hived_status("finished replaying");

  if( replay_is_last_operation )
  {
    if( !appbase::app().is_interrupt_request() )
    {
      ilog("Generating artificial interrupt request...");

      /*
        Triggering artificial signal.
        Whole application should be closed in identical way, as if it was closed by user.
        This case occurs only when `exit-after-replay` switch is used.
      */
      appbase::app().generate_interrupt_request();
    }
  }
  else
  {
    //if `stop_replay_at` > 0 stay in API node context( without synchronization )
    if( stop_replay_at > 0 )
      is_p2p_enabled = false;
  }

  return replay_is_last_operation;
}

void chain_plugin_impl::initial_settings()
{
  if( statsd_on_replay )
  {
    auto statsd = appbase::app().find_plugin< hive::plugins::statsd::statsd_plugin >();
    if( statsd != nullptr )
    {
      statsd->start_logging();
    }
  }

  ilog( "Starting chain with shared_file_size: ${n} bytes", ("n", shared_memory_size) );

  if(resync)
  {
    wlog("resync requested: deleting block log and shared memory");
    db.wipe( app().data_dir() / "blockchain", shared_memory_dir, true );
  }

  db.set_flush_interval( flush_interval );
  db.add_checkpoints( loaded_checkpoints );
  db.set_require_locking( check_locks );

  const auto& abstract_index_cntr = db.get_abstract_index_cntr();

  get_indexes_memory_details = [ this, &abstract_index_cntr ]
    (index_memory_details_cntr_t& index_memory_details_cntr, bool onlyStaticInfo)
  {
    for (auto idx : abstract_index_cntr)
    {
      auto info = idx->get_statistics(onlyStaticInfo);
      index_memory_details_cntr.emplace_back(std::move(info._value_type_name), info._item_count,
        info._item_sizeof, info._item_additional_allocation, info._additional_container_allocation);
    }
  };

  fc::variant database_config;

  db_open_args.data_dir = app().data_dir() / "blockchain";
  db_open_args.shared_mem_dir = shared_memory_dir;
  db_open_args.initial_supply = HIVE_INIT_SUPPLY;
  db_open_args.hbd_initial_supply = HIVE_HBD_INIT_SUPPLY;
  db_open_args.shared_file_size = shared_memory_size;
  db_open_args.shared_file_full_threshold = shared_file_full_threshold;
  db_open_args.shared_file_scale_rate = shared_file_scale_rate;
  db_open_args.chainbase_flags = chainbase_flags;
  db_open_args.do_validate_invariants = validate_invariants;
  db_open_args.stop_replay_at = stop_replay_at;
  db_open_args.exit_after_replay = exit_after_replay;
  db_open_args.force_replay = force_replay;
  db_open_args.validate_during_replay = validate_during_replay;
  db_open_args.benchmark_is_enabled = benchmark_is_enabled;
  db_open_args.database_cfg = database_config;
  db_open_args.replay_in_memory = replay_in_memory;
  db_open_args.replay_memory_indices = replay_memory_indices;
  db_open_args.enable_block_log_compression = enable_block_log_compression;
  db_open_args.block_log_compression_level = block_log_compression_level;
}

bool chain_plugin_impl::check_data_consistency()
{
  uint64_t head_block_num_origin = 0;
  uint64_t head_block_num_state = 0;

  auto _is_reindex_complete = db.is_reindex_complete( &head_block_num_origin, &head_block_num_state );

  if( !_is_reindex_complete )
  {
    if( head_block_num_state > head_block_num_origin )
    {
      appbase::app().generate_interrupt_request();
      return false;
    }
    if( db.get_snapshot_loaded() )
    {
      wlog( "Replaying has to be forced, after snapshot's loading. { \"block_log-head\": ${b1}, \"state-head\": ${b2} }", ( "b1", head_block_num_origin )( "b2", head_block_num_state ) );
    }
    else
    {
      wlog( "Replaying is not finished. Synchronization is not allowed. { \"block_log-head\": ${b1}, \"state-head\": ${b2} }", ( "b1", head_block_num_origin )( "b2", head_block_num_state ) );
      appbase::app().generate_interrupt_request();
      return false;
    }
  }

  return true;
}

void chain_plugin_impl::open()
{
  try
  {
    ilog("Opening shared memory from ${path}", ("path",shared_memory_dir.generic_string()));

    db.open( db_open_args );

    if( dump_memory_details )
    {
      setup_benchmark_dumper();
      dumper.dump( true, get_indexes_memory_details );
    }
  }
  catch( const fc::exception& e )
  {
    wlog( "Error opening database. If the binary or configuration has changed, replay the blockchain explicitly using `--force-replay`." );
    wlog( "If you know what you are doing you can skip this check and force open the database using `--force-open`." );
    wlog( "WARNING: THIS MAY CORRUPT YOUR DATABASE. FORCE OPEN AT YOUR OWN RISK." );
    wlog( " Error: ${e}", ("e", e) );
    hive::notify_hived_status("exitting with open database error");
    exit(EXIT_FAILURE);
  }
}

bool chain_plugin_impl::replay_blockchain()
{
  try
  {
    ilog("Replaying blockchain on user request.");
    uint32_t last_block_number = 0;
    last_block_number = db.reindex( db_open_args );

    if( benchmark_interval > 0 )
    {
      const hive::utilities::benchmark_dumper::measurement& total_data = dumper.dump( dump_memory_details, get_indexes_memory_details );
      ilog( "Performance report (total). Blocks: ${b}. Elapsed time: ${rt} ms (real), ${ct} ms (cpu). Memory usage: ${cm} (current), ${pm} (peak) kilobytes.",
          ("b", total_data.block_number)
          ("rt", total_data.real_ms)
          ("ct", total_data.cpu_ms)
          ("cm", total_data.current_mem)
          ("pm", total_data.peak_mem) );
    }

    if( stop_replay_at > 0 && stop_replay_at == last_block_number )
    {
      ilog("Stopped blockchain replaying on user request. Last applied block number: ${n}.", ("n", last_block_number));
    }

    /*
      Returns information if the replay is last operation.
    */
    return appbase::app().is_interrupt_request()/*user triggered SIGINT/SIGTERM*/ || exit_after_replay/*shutdown node definitely*/;
  } FC_CAPTURE_AND_LOG( () )

  return true;
}

void chain_plugin_impl::process_snapshot()
{
  if( snapshot_provider != nullptr )
    snapshot_provider->process_explicit_snapshot_requests( db_open_args );
}

void chain_plugin_impl::work( synchronization_type& on_sync )
{
  ilog( "Started on blockchain with ${n} blocks, LIB: ${lb}", ("n", db.head_block_num())("lb", db.get_last_irreversible_block_num()) );
  if(this->exit_before_sync)
  {
    ilog("Shutting down node without performing any action on user request");
    appbase::app().generate_interrupt_request();
    return;
  }else ilog( "Started on blockchain with ${n} blocks", ("n", db.head_block_num()) );

  on_sync();

  start_write_processing();
}

void chain_plugin_impl::write_default_database_config( bfs::path &p )
{
  ilog( "writing database configuration: ${p}", ("p", p.string()) );
  fc::json::save_to_file( hive::utilities::default_database_configuration(), p );
}

void chain_plugin_impl::setup_benchmark_dumper()
{
  if(!this->dumper.is_initialized())
  {
    typedef hive::utilities::benchmark_dumper::database_object_sizeof_cntr_t database_object_sizeof_cntr_t;
    const auto abstract_index_cntr = this->db.get_abstract_index_cntr();
    auto get_database_objects_sizeofs = [ this, &abstract_index_cntr ]
      (database_object_sizeof_cntr_t& database_object_sizeof_cntr)
    {
      database_object_sizeof_cntr.reserve(abstract_index_cntr.size());
      for (auto idx : abstract_index_cntr)
      {
        auto info = idx->get_statistics(true);
        database_object_sizeof_cntr.emplace_back(std::move(info._value_type_name), info._item_sizeof);
      }
    };
    this->dumper.initialize( get_database_objects_sizeofs, (this->dump_memory_details ? BENCHMARK_FILE_NAME : "") );
  }
}

} // detail


chain_plugin::chain_plugin() : my( new detail::chain_plugin_impl() ) {}
chain_plugin::~chain_plugin(){}

database& chain_plugin::db() { return my->db; }
const hive::chain::database& chain_plugin::db() const { return my->db; }

bfs::path chain_plugin::state_storage_dir() const
{
  return my->shared_memory_dir;
}

void chain_plugin::set_program_options(options_description& cli, options_description& cfg)
{
#ifdef USE_ALTERNATE_CHAIN_ID
  using hive::protocol::testnet_blockchain_configuration::configuration_data;
  /// Default initminer priv. key in non-mainnet builds.
  auto default_skeleton_privkey = hive::utilities::key_to_wif(configuration_data.get_default_initminer_private_key());
#endif

  cfg.add_options()
      ("shared-file-dir", bpo::value<bfs::path>()->default_value("blockchain")->value_name("dir"), // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
        "the location of the chain shared memory files (absolute path or relative to application data dir)")
      ("shared-file-size", bpo::value<string>()->default_value("24G"), "Size of the shared memory file. Default: 24G. If running with many plugins, increase this value to 28G.")
      ("shared-file-full-threshold", bpo::value<uint16_t>()->default_value(0),
        "A 2 precision percentage (0-10000) that defines the threshold for when to autoscale the shared memory file. Setting this to 0 disables autoscaling. Recommended value for consensus node is 9500 (95%)." )
      ("shared-file-scale-rate", bpo::value<uint16_t>()->default_value(0),
        "A 2 precision percentage (0-10000) that defines how quickly to scale the shared memory file. When autoscaling occurs the file's size will be increased by this percent. Setting this to 0 disables autoscaling. Recommended value is between 1000-2000 (10-20%)" )
      ("checkpoint,c", bpo::value<vector<string>>()->composing(), "Pairs of [BLOCK_NUM,BLOCK_ID] that should be enforced as checkpoints.")
      ("flush-state-interval", bpo::value<uint32_t>(),
        "flush shared memory changes to disk every N blocks")
      ("enable-block-log-compression", boost::program_options::value<bool>()->default_value(true), "Compress blocks using zstd as they're added to the block log" )
      ("block-log-compression-level", bpo::value<int>()->default_value(15), "Block log zstd compression level 0 (fast, low compression) - 22 (slow, high compression)" )
      ("blockchain-thread-pool-size", bpo::value<uint32_t>()->default_value(8)->value_name("size"), "Number of worker threads used to pre-validate transactions and blocks")
      ("block-stats-report-type", bpo::value<string>()->default_value("FULL"), "Level of detail of block stat reports: NONE, MINIMAL, REGULAR, FULL. Default FULL (recommended for API nodes)." )
      ("block-stats-report-output", bpo::value<string>()->default_value("ILOG"), "Where to put block stat reports: DLOG, ILOG, NOTIFY. Default ILOG." )
      ;
  cli.add_options()
      ("replay-blockchain", bpo::bool_switch()->default_value(false), "clear chain database and replay all blocks" )
      ("force-open", bpo::bool_switch()->default_value(false), "force open the database, skipping the environment check" )
      ("resync-blockchain", bpo::bool_switch()->default_value(false), "clear chain database and block log" )
      ("stop-replay-at-block", bpo::value<uint32_t>(), "Stop after reaching given block number")
      ("exit-after-replay", bpo::bool_switch()->default_value(false), "[ DEPRECATED ] Exit after reaching given block number")
      ("exit-before-sync", bpo::bool_switch()->default_value(false), "Exits before starting sync, handy for dumping snapshot without starting replay")
      ("force-replay", bpo::bool_switch()->default_value(false), "Before replaying clean all old files. If specifed, `--replay-blockchain` flag is implied")
      ("validate-during-replay", bpo::bool_switch()->default_value(false), "Runs all validations that are normally turned off during replay")
      ("advanced-benchmark", "Make profiling for every plugin.")
      ("set-benchmark-interval", bpo::value<uint32_t>(), "Print time and memory usage every given number of blocks")
      ("dump-memory-details", bpo::bool_switch()->default_value(false), "Dump database objects memory usage info. Use set-benchmark-interval to set dump interval.")
      ("check-locks", bpo::bool_switch()->default_value(false), "Check correctness of chainbase locking" )
      ("validate-database-invariants", bpo::bool_switch()->default_value(false), "Validate all supply invariants check out" )
#ifdef USE_ALTERNATE_CHAIN_ID
      ("chain-id", bpo::value< std::string >()->default_value( HIVE_CHAIN_ID ), "chain ID to connect to")
      ("skeleton-key", bpo::value< std::string >()->default_value(default_skeleton_privkey), "WIF PRIVATE key to be used as skeleton key for all accounts")
#endif
      ;
}

void chain_plugin::plugin_initialize(const variables_map& options) {
  hive::utilities::notifications::setup_notifications(options);
  my->shared_memory_dir = app().data_dir() / "blockchain";

  if( options.count("shared-file-dir") )
  {
    auto sfd = options.at("shared-file-dir").as<bfs::path>();
    if(sfd.is_relative())
      my->shared_memory_dir = app().data_dir() / sfd;
    else
      my->shared_memory_dir = sfd;
  }

  my->shared_memory_size = fc::parse_size( options.at( "shared-file-size" ).as< string >() );

  if( options.count( "shared-file-full-threshold" ) )
    my->shared_file_full_threshold = options.at( "shared-file-full-threshold" ).as< uint16_t >();

  if( options.count( "shared-file-scale-rate" ) )
    my->shared_file_scale_rate = options.at( "shared-file-scale-rate" ).as< uint16_t >();

  my->chainbase_flags |= options.at( "force-open" ).as< bool >() ? chainbase::skip_env_check : chainbase::skip_nothing;

  my->force_replay        = options.count( "force-replay" ) ? options.at( "force-replay" ).as<bool>() : false;
  my->validate_during_replay =
    options.count( "validate-during-replay" ) ? options.at( "validate-during-replay" ).as<bool>() : false;
  my->replay              = options.at( "replay-blockchain").as<bool>() || my->force_replay;
  my->resync              = options.at( "resync-blockchain").as<bool>();
  my->stop_replay_at      = options.count( "stop-replay-at-block" ) ? options.at( "stop-replay-at-block" ).as<uint32_t>() : 0;
  my->exit_before_sync    = options.count( "exit-before-sync" ) ? options.at( "exit-before-sync" ).as<bool>() : false;
  my->benchmark_interval  =
    options.count( "set-benchmark-interval" ) ? options.at( "set-benchmark-interval" ).as<uint32_t>() : 0;
  my->check_locks         = options.at( "check-locks" ).as< bool >();
  my->validate_invariants = options.at( "validate-database-invariants" ).as<bool>();
  my->dump_memory_details = options.at( "dump-memory-details" ).as<bool>();
  my->enable_block_log_compression = options.at( "enable-block-log-compression" ).as<bool>();
  my->block_log_compression_level = options.at( "block-log-compression-level" ).as<int>();
  if( options.count( "flush-state-interval" ) )
    my->flush_interval = options.at( "flush-state-interval" ).as<uint32_t>();
  else
    my->flush_interval = 10000;

  if(options.at( "exit-after-replay" ).as<bool>())
  {
    my->exit_after_replay = true;
    wlog("flag `--exit-after-replay` is deprecated, please consider usage of `--exit-before-sync` flag instead");
  }

  if(options.count("checkpoint"))
  {
    auto cps = options.at("checkpoint").as<vector<string>>();
    my->loaded_checkpoints.reserve(cps.size());
    for(const auto& cp : cps)
    {
      auto item = fc::json::from_string(cp).as<std::pair<uint32_t,block_id_type>>();
      my->loaded_checkpoints[item.first] = item.second;
    }
  }

  this->my->setup_benchmark_dumper();
  my->benchmark_is_enabled = (options.count( "advanced-benchmark" ) != 0);

  if( options.count( "statsd-record-on-replay" ) )
  {
    my->statsd_on_replay = options.at( "statsd-record-on-replay" ).as< bool >();
  }

#ifdef USE_ALTERNATE_CHAIN_ID
  if( options.count( "chain-id" ) )
  {
    auto chain_id_str = options.at("chain-id").as< std::string >();

    try
    {
      my->db.set_chain_id( chain_id_type( chain_id_str) );
    }
    catch( const fc::exception& )
    {
      FC_ASSERT( false, "Could not parse chain_id as hex string. Chain ID String: ${s}", ("s", chain_id_str) );
    }

    if(options.count("skeleton-key"))
    {

      const std::string& wif_key = options.at("skeleton-key").as<std::string>();

      wlog("Setting custom skeleton key: ${sk}", ("sk", wif_key));

      fc::optional<fc::ecc::private_key> skeleton_key = hive::utilities::wif_to_key(wif_key);
      FC_ASSERT(skeleton_key.valid(), "unable to parse passed skeletpn key: ${sk}", ("sk", wif_key));

      configuration_data.set_skeleton_key(*skeleton_key);
    }

  }
#endif
  uint32_t blockchain_thread_pool_size = options.at("blockchain-thread-pool-size").as<uint32_t>();
  blockchain_worker_thread_pool::set_thread_pool_size(blockchain_thread_pool_size);

  block_flow_control::set_auto_report(options.at("block-stats-report-type").as<std::string>(),
                                      options.at("block-stats-report-output").as<std::string>());

  if(my->benchmark_interval > 0)
  {
    my->dumper_post_apply_block = db().add_post_apply_block_handler([&](const hive::chain::block_notification& note) noexcept {
      const uint32_t current_block_number = note.block_num;
      if( my->benchmark_interval && ( current_block_number % my->benchmark_interval == 0 ) )
      {
        const hive::utilities::benchmark_dumper::measurement& measure =
          my->dumper.measure( current_block_number, my->get_indexes_memory_details );

        ilog( "Performance report at block ${n}. Elapsed time: ${rt} ms (real), ${ct} ms (cpu). Memory usage: ${cm} (current), ${pm} (peak) kilobytes.",
          ("n", current_block_number)
          ("rt", measure.real_ms)
          ("ct", measure.cpu_ms)
          ("cm", measure.current_mem)
          ("pm", measure.peak_mem) );

        hive::notify("hived_benchmark", "multiindex_stats", fc::variant{measure});
      }
    }, *this, 0);
  }
}

void chain_plugin::plugin_startup()
{
  ilog("Chain plugin initialization...");

  my->initial_settings();

  ilog("Database opening...");
  my->open();

  ilog("Looking for snapshot processing requests...");
  my->process_snapshot();


  if( my->replay )
  {
    ilog("Replaying...");
    if( !my->start_replay_processing() )
    {
      ilog("P2P enabling after replaying...");
      my->work( on_sync );
    }
  }
  else
  {
    ilog("Consistency data checking...");
    if( my->check_data_consistency() )
    {
      if( my->db.get_snapshot_loaded() )
      {
        ilog("Replaying...");
        //Replaying is forced, because after snapshot loading, node should work in synchronization mode.
        if( !my->start_replay_processing() )
        {
          ilog("P2P enabling after replaying...");
          my->work( on_sync );
        }
      }
      else
      {
        ilog("P2P enabling...");
        my->work( on_sync );
      }
    }
  }

  ilog("Chain plugin initialization finished...");
}

void chain_plugin::plugin_shutdown()
{
  ilog("closing chain database");
  blockchain_worker_thread_pool::get_instance().shutdown();
  my->stop_write_processing();
  my->db.close();
  ilog("database closed successfully");
  hive::notify_hived_status("finished syncing");
}

void chain_plugin::register_snapshot_provider(state_snapshot_provider& provider)
{
  my->register_snapshot_provider(provider);
}

void chain_plugin::report_state_options( const string& plugin_name, const fc::variant_object& opts )
{
  my->loaded_plugins.push_back( plugin_name );
  my->plugin_state_opts( opts );
}

void chain_plugin::connection_count_changed(uint32_t peer_count)
{
  my->peer_count = peer_count;
  fc_wlog(fc::logger::get("default"),"peer_count changed: ${peer_count}",(peer_count));
}

bool chain_plugin::accept_block( const std::shared_ptr< p2p_block_flow_control >& block_ctrl, bool currently_syncing )
{
  const signed_block& block = block_ctrl->get_full_block()->get_block();
  if (currently_syncing && block.block_num() % 10000 == 0)
  {
    fc::time_point now = fc::time_point::now();
    if (my->last_myriad_time)
    {
      fc::microseconds duration = now - *my->last_myriad_time;
      float microseconds_per_block = (float)duration.count() / 10000.f;
      std::ostringstream microseconds_per_block_stream;
      microseconds_per_block_stream << std::setprecision(2) << std::fixed << microseconds_per_block;
      ilog("Syncing Blockchain --- Got block: #${n} time: ${t} producer: ${p} --- ${microseconds_per_block}µs/block",
           ("t", block.timestamp)("n", block.block_num())("p", block.witness)
           ("microseconds_per_block", microseconds_per_block_stream.str()));
    }
    else
      ilog("Syncing Blockchain --- Got block: #${n} time: ${t} producer: ${p}",
           ("t", block.timestamp)("n", block.block_num())("p", block.witness));
#define DISPLAY_SYNC_SPEED
#ifdef DISPLAY_SYNC_SPEED
    my->last_myriad_time = now;
#endif
  }

  check_time_in_block(block);

  write_context cxt;
  cxt.req_ptr = block_ctrl;
  static int call_count = 0;
  call_count++;
  BOOST_SCOPE_EXIT(&call_count) {
    --call_count;
    fc_dlog(fc::logger::get("chainlock"), "<-- accept_block_calls_in_progress: ${call_count}", (call_count));
  } BOOST_SCOPE_EXIT_END

  fc_dlog(fc::logger::get("chainlock"), "--> fc accept_block_calls_in_progress: ${call_count}", (call_count));
  fc::promise<void>::ptr accept_block_promise(new fc::promise<void>("accept_block"));
  fc::future<void> accept_block_future(accept_block_promise);
  block_ctrl->attach_promise( accept_block_promise );
  {
    std::unique_lock<std::mutex> lock(my->queue_mutex);
    my->write_queue.push(&cxt);
  }
  my->queue_condition_variable.notify_one();
  accept_block_future.wait();

  block_ctrl->rethrow_if_exception();

  return block_ctrl->forked();
}

void chain_plugin::accept_transaction( const std::shared_ptr<full_transaction_type>& full_transaction, const lock_type lock /* = lock_type::boost */  )
{
  transaction_flow_control tx_ctrl( full_transaction );
  write_context cxt;
  cxt.req_ptr = &tx_ctrl;
  static int call_count = 0;
  call_count++;
  BOOST_SCOPE_EXIT(&call_count) {
    --call_count;
    fc_dlog(fc::logger::get("chainlock"), "<-- accept_transaction_calls_in_progress: ${call_count}", (call_count));
  } BOOST_SCOPE_EXIT_END
  
  if (lock == lock_type::boost)
  {
    fc_dlog(fc::logger::get("chainlock"), "--> boost accept_transaction_calls_in_progress: ${call_count}", (call_count));
    std::shared_ptr<boost::promise<void>> accept_transaction_promise = std::make_shared<boost::promise<void>>();
    boost::unique_future<void> accept_transaction_future(accept_transaction_promise->get_future());
    tx_ctrl.attach_promise( accept_transaction_promise );
    {
      std::unique_lock<std::mutex> lock(my->queue_mutex);
      my->write_queue.push(&cxt);
    }
    my->queue_condition_variable.notify_one();
    accept_transaction_future.get();
  }
  else
  {
    fc_dlog(fc::logger::get("chainlock"), "--> fc accept_transaction_calls_in_progress: ${call_count}", (call_count));
    fc::promise<void>::ptr accept_transaction_promise(new fc::promise<void>("accept_transaction"));
    fc::future<void> accept_transaction_future(accept_transaction_promise);
    tx_ctrl.attach_promise( accept_transaction_promise );
    {
      std::unique_lock<std::mutex> lock(my->queue_mutex);
      my->write_queue.push(&cxt);
    }
    my->queue_condition_variable.notify_one();
    accept_transaction_future.wait();
  }

  tx_ctrl.rethrow_if_exception();
}

void chain_plugin::determine_encoding_and_accept_transaction( full_transaction_ptr& result, const hive::protocol::signed_transaction& trx,
  std::function< void( bool hf26_auth_fail )> on_full_trx, const lock_type lock /* = lock_type::boost */)
{ try {
  result = full_transaction_type::create_from_signed_transaction( trx, hive::protocol::pack_type::hf26, true /* cache this transaction */);
  on_full_trx( false );
  // the only reason we'd be getting something in singed_transaction form is from the API, coming in as json
  blockchain_worker_thread_pool::get_instance().enqueue_work(result, blockchain_worker_thread_pool::data_source_type::standalone_transaction_received_from_api);
  try
  {
    accept_transaction(result, lock);
  }
  catch (const hive::protocol::transaction_auth_exception&)
  {
    on_full_trx( true );
    result = full_transaction_type::create_from_signed_transaction( trx, hive::protocol::pack_type::legacy, true /* cache this transaction */);
    on_full_trx( false );
    blockchain_worker_thread_pool::get_instance().enqueue_work(result, blockchain_worker_thread_pool::data_source_type::standalone_transaction_received_from_api);
    try
    {
      accept_transaction(result, lock);
    }
    catch (hive::protocol::transaction_auth_exception& legacy_e)
    {
      FC_RETHROW_EXCEPTION(legacy_e, error, "Transaction failed to validate using both new (hf26) and legacy serialization");
    }
  }
} FC_CAPTURE_AND_RETHROW() }


void chain_plugin::generate_block( const std::shared_ptr< generate_block_flow_control >& generate_block_ctrl )
{
  write_context cxt;
  cxt.req_ptr = generate_block_ctrl;

  std::shared_ptr<boost::promise<void>> generate_block_promise = std::make_shared<boost::promise<void>>();
  boost::unique_future<void> generate_block_future(generate_block_promise->get_future());
  generate_block_ctrl->attach_promise( generate_block_promise );

  {
    std::unique_lock<std::mutex> lock(my->queue_mutex);
    my->write_queue.push(&cxt);
  }
  my->queue_condition_variable.notify_one();

  generate_block_future.get();

  generate_block_ctrl->rethrow_if_exception();
}

int16_t chain_plugin::set_write_lock_hold_time( int16_t new_time )
{
  FC_ASSERT( get_state() == appbase::abstract_plugin::state::initialized,
    "Can only change write_lock_hold_time while chain_plugin is initialized." );

  int16_t old_time = my->write_lock_hold_time;
  my->write_lock_hold_time = new_time;
  return old_time;
}

bool chain_plugin::block_is_on_preferred_chain(const hive::chain::block_id_type& block_id )
{
  // If it's not known, it's not preferred.
  if( !db().is_known_block(block_id) ) return false;

  // Extract the block number from block_id, and fetch that block number's ID from the database.
  // If the database's block ID matches block_id, then block_id is on the preferred chain. Otherwise, it's on a fork.
  return db().get_block_id_for_num( hive::chain::block_header::num_from_id( block_id ) ) == block_id;
}

void chain_plugin::check_time_in_block(const hive::chain::signed_block& block)
{
  const fc::time_point_sec max_accept_time = fc::time_point_sec(fc::time_point::now()) + my->allow_future_time;
  FC_ASSERT(block.timestamp <= max_accept_time, "Refusing to accept block that's too far in the future (> ${max_accept_time})", (max_accept_time));
}

void chain_plugin::register_block_generator( const std::string& plugin_name, std::shared_ptr< abstract_block_producer > block_producer )
{
  FC_ASSERT( get_state() == appbase::abstract_plugin::state::initialized, "Can only register a block generator when the chain_plugin is initialized." );

  if ( my->block_generator )
    wlog( "Overriding a previously registered block generator by: ${registrant}", ("registrant", my->block_generator_registrant) );

  my->block_generator_registrant = plugin_name;
  my->block_generator = std::move( block_producer );
}

bool chain_plugin::is_p2p_enabled() const
{
  return my->is_p2p_enabled;
}

} } } // namespace hive::plugis::chain::chain_apis

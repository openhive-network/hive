#include <hive/plugins/chain/chain_plugin.hpp>

#include <appbase/application.hpp>

#include <hive/chain/blockchain_worker_thread_pool.hpp>
#include <hive/chain/block_storage_interface.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/db_with.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/irreversible_block_writer.hpp>
#include <hive/chain/sync_block_writer.hpp>

#include <hive/plugins/chain/abstract_block_producer.hpp>
#include <hive/plugins/chain/state_snapshot_provider.hpp>
#include <hive/plugins/statsd/utility.hpp>

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
#include <algorithm>

namespace hive { namespace plugins { namespace chain {

#define BENCHMARK_FILE_NAME "replay_benchmark.json"

using namespace hive;

using fc::flat_map;
using hive::chain::block_id_type;

using hive::plugins::chain::synchronization_type;
using index_memory_details_cntr_t = hive::utilities::benchmark_dumper::index_memory_details_cntr_t;
using get_indexes_memory_details_type = std::function< void( index_memory_details_cntr_t& ) >;

#define NUM_THREADS 1

typedef fc::static_variant<std::shared_ptr<boost::promise<void>>, fc::promise<void>::ptr> promise_ptr;

class transaction_flow_control
{
public:
  explicit transaction_flow_control( const std::shared_ptr<full_transaction_type>& _tx ) : tx( _tx ) {}

  void attach_promise( promise_ptr _p ) { prom_ptr = _p; }

  void on_failure( const fc::exception& ex ) { except = ex.dynamic_copy_exception(); }
  void on_worker_done( appbase::application& app );

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

void transaction_flow_control::on_worker_done( appbase::application& app )
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
    chain_plugin_impl( appbase::application& app ):
      thread_pool( app ),
      db( app ),
      webserver( app.get_plugin<hive::plugins::webserver::webserver_plugin>() ),
      theApp( app )
    {}

    ~chain_plugin_impl()
    {
      stop_write_processing();

      if( chain_sync_con.connected() )
        chain_sync_con.disconnect();
    }

    void register_snapshot_provider(state_snapshot_provider& provider)
    {
      snapshot_provider = &provider;
    }

    bool is_interrupt_request() const;
    bool is_running() const;
    fc::microseconds get_time_gap_to_live_sync( const fc::time_point_sec& head_block_time );

    void start_write_processing();
    void stop_write_processing();

    bool start_replay_processing( std::shared_ptr< block_write_i > reindex_block_writer,
                                  hive::chain::blockchain_worker_thread_pool& thread_pool );

    void initial_settings();
    void open();
    bool replay_blockchain( const block_read_i& block_reader, hive::chain::blockchain_worker_thread_pool& thread_pool );
    void process_snapshot();
    bool check_data_consistency( const block_read_i& block_reader );

    void prepare_work( bool started, synchronization_type& on_sync );
    void work( synchronization_type& on_sync );

    void write_default_database_config( bfs::path& p );
    void setup_benchmark_dumper();

    void push_transaction( const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip );
    bool push_block( const block_flow_control& block_ctrl, uint32_t skip );

    void add_checkpoints( const flat_map< uint32_t, block_id_type >& checkpts, hive::chain::blockchain_worker_thread_pool& thread_pool );
    const flat_map<uint32_t,block_id_type> get_checkpoints()const { return checkpoints; }
    bool before_last_checkpoint()const;

    void finish_request();

    using block_log_open_args=block_storage_i::block_log_open_args;

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
    uint32_t                         stop_at_block = 0;
    uint32_t                         exit_at_block = 0;
    bool                             exit_after_replay = false;
    bool                             exit_before_sync = false;
    bool                             force_replay = false;
    bool                             validate_during_replay = false;
    uint32_t                         benchmark_interval = 0;
    uint32_t                         flush_interval = 0;
    bool                             replay_in_memory = false;
    std::vector< std::string >       replay_memory_indices{};
    bool                             enable_block_log_compression = true;
    bool                             enable_block_log_auto_fixing = true;
    bool                             load_snapshot = false;
    int                              block_log_compression_level = 15;
    flat_map<uint32_t,block_id_type> checkpoints;
    flat_map<uint32_t,block_id_type> loaded_checkpoints;
    bool                             last_pushed_block_was_before_checkpoint = false; // just used for logging
    bool                             stop_at_block_interrupt_request = false;


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

    hive::chain::blockchain_worker_thread_pool thread_pool;

    database                            db;
    std::shared_ptr<block_storage_i>    block_storage;
    std::unique_ptr<sync_block_writer>  default_block_writer;

    std::string block_generator_registrant;
    std::shared_ptr< abstract_block_producer > block_generator;

    hive::utilities::benchmark_dumper   dumper;
    hive::chain::open_args              db_open_args;
    block_log_open_args                 bl_open_args;
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

    struct
    {
      /// Flag should be set to true, to allow immediate exit on scenarios where writer thread didn't start yet
      std::atomic_bool        status{true};
      std::mutex              mtx;
      std::condition_variable cv;
    } finish;

    struct sync_progress_data
    {
      fc::time_point last_myriad_time;
      uint64_t       total_block_size;
      uint64_t       total_tx_count;
    };
    fc::optional<sync_progress_data> sync_progress;

    class write_request_visitor;

    boost::signals2::connection                 chain_sync_con;
    hive::plugins::webserver::webserver_plugin& webserver;

    appbase::application& theApp;

  private:
    bool _push_block( const block_flow_control& block_ctrl );

    uint32_t reindex( const open_args& args, const block_read_i& block_reader, hive::chain::blockchain_worker_thread_pool& thread_pool );
    uint32_t reindex_internal( const open_args& args,
      const std::shared_ptr<full_block_type>& start_block, const block_read_i& block_reader, hive::chain::blockchain_worker_thread_pool& thread_pool );
    /**
      * @brief Check if replaying was finished and all blocks from `block_reader` were processed.
      *
      * If returns `true` then a synchronization is allowed.
      * If returns `false`, then opening a node should be forbidden.
      *
      * There are output-type arguments: `head_block_num_origin`, `head_block_num_state` for information purposes only.
      *
      * @return information if replaying was finished
      */
    bool is_reindex_complete( uint64_t* head_block_num_origin, uint64_t* head_block_num_state,
                              const block_read_i& block_reader ) const;
};

struct chain_plugin_impl::write_request_visitor
{
  write_request_visitor( chain_plugin_impl& _chain_plugin ) : cp( _chain_plugin ) {}

  uint32_t         last_block_number = 0;
  //cumulative data on transactions since last block
  uint32_t         count_tx_pushed = 0;
  uint32_t         count_tx_applied = 0;
  uint32_t         count_tx_failed_auth = 0;
  uint32_t         count_tx_failed_no_rc = 0;

  chain_plugin_impl& cp;
  write_context* cxt = nullptr;

  using result_type = uint32_t;

  void on_block( const block_flow_control* block_ctrl )
  {
    block_ctrl->on_write_queue_pop( count_tx_pushed, count_tx_applied, count_tx_failed_auth, count_tx_failed_no_rc );
    count_tx_pushed = 0;
    count_tx_applied = 0;
    count_tx_failed_auth = 0;
    count_tx_failed_no_rc = 0;
  }

  uint32_t operator()( std::shared_ptr< p2p_block_flow_control > p2p_block_ctrl )
  {
    try
    {
      if( cp.is_interrupt_request() )
      {
        throw fc::exception(fc::canceled_exception_code, "interrupted by user");
      }

      STATSD_START_TIMER("chain", "write_time", "push_block", 1.0f, cp.theApp)
      on_block( p2p_block_ctrl.get() );
      fc::time_point time_before_pushing_block = fc::time_point::now();
      BOOST_SCOPE_EXIT(this_, time_before_pushing_block, &statsd_timer) {
        this_->cp.cumulative_time_processing_blocks += fc::time_point::now() - time_before_pushing_block;
        STATSD_STOP_TIMER("chain", "write_time", "push_block")
      } BOOST_SCOPE_EXIT_END
      cp.push_block( *p2p_block_ctrl.get(), p2p_block_ctrl->get_skip_flags() );
      last_block_number = p2p_block_ctrl->get_full_block()->get_block_num();
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
    p2p_block_ctrl->on_worker_done( cp.theApp );
    return last_block_number;
  }

  uint32_t operator()( transaction_flow_control* tx_ctrl )
  {
    try
    {
      STATSD_START_TIMER( "chain", "write_time", "push_transaction", 1.0f, cp.theApp)
      fc::time_point time_before_pushing_transaction = fc::time_point::now();
      ++count_tx_pushed;
      cp.push_transaction( tx_ctrl->get_full_transaction(), database::skip_nothing );
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
    tx_ctrl->on_worker_done( cp.theApp );
    return last_block_number;
  }

  uint32_t operator()( std::shared_ptr< generate_block_flow_control > generate_block_ctrl )
  {
    try
    {
      if( !cp.block_generator )
        FC_THROW_EXCEPTION( chain_exception, "Received a generate block request, but no block generator has been registered." );

      STATSD_START_TIMER( "chain", "write_time", "generate_block", 1.0f, cp.theApp )
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
    generate_block_ctrl->on_worker_done( cp.theApp );
    return last_block_number;
  }
};

bool chain_plugin_impl::is_interrupt_request() const
{
  return theApp.is_interrupt_request() || stop_at_block_interrupt_request;
}

bool chain_plugin_impl::is_running() const
{
  return running && !is_interrupt_request();
}

fc::microseconds chain_plugin_impl::get_time_gap_to_live_sync( const fc::time_point_sec& head_block_time )
{
  return fc::time_point::now() - head_block_time - fc::minutes(1);
}

void chain_plugin_impl::start_write_processing()
{
  write_processor_thread = std::make_shared<std::thread>([&]()
  {
    try
    {
      BOOST_SCOPE_EXIT(this_)
      {
        {
          std::unique_lock<std::mutex> _guard(this_->finish.mtx);
          this_->finish.status = true;
        }
        /// Notify waiting (main-thread) that starting shutdown procedure is allowed, since main-writer thread activity finished.
        this_->finish.cv.notify_one();
      } BOOST_SCOPE_EXIT_END

      finish.status = false;

      uint32_t last_block_number = 0;
      theApp.notify_status("syncing");
      ilog("Write processing thread started.");
      fc::set_thread_name("write_queue");
      fc::thread::current().set_name("write_queue");
      cumulative_times_last_reported_time = fc::time_point::now();

      const int64_t nr_seconds = 10 * HIVE_BLOCK_INTERVAL;
      const fc::microseconds block_wait_max_time = fc::seconds(nr_seconds);

      const int64_t time_fragments = nr_seconds * 2;

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

      theApp.notify_status("syncing");
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

          //divide `max_time_to_wait` into smaller fragments (~500ms) in order to check faster `is_running` status
          int64_t chunk_time = max_time_to_wait.count() / time_fragments;
          while (is_running() && write_queue.empty() && !wait_timed_out)
          {
            size_t cnt = 0;
            size_t wait_timed_out_cnt = 0;
            while( cnt < time_fragments && is_running() && write_queue.empty() && !wait_timed_out )
            {
              if( queue_condition_variable.wait_for(lock, std::chrono::microseconds(chunk_time)) == std::cv_status::timeout )
              {
                ++wait_timed_out_cnt;
              }

              ++cnt;
              wait_timed_out = wait_timed_out_cnt == time_fragments;
            }
          }

          if (!is_running()) // we woke because the node is shutting down
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
          STATSD_START_TIMER( "chain", "lock_time", "write_lock", 1.0f, theApp )
          while (true)
          {
            req_visitor.cxt = cxt;
            last_block_number = cxt->req_ptr.visit( req_visitor );

            ++write_queue_items_processed;

            if( stop_at_block > 0 && stop_at_block == last_block_number )
            {
              ilog("Stopped ${mode} on user request. Last applied block number: ${n}.", ("n", last_block_number)("mode", is_syncing ? "syncing" : "live mode"));
              stop_at_block_interrupt_request = true;
            }

            if (!is_syncing) //if not syncing, we shouldn't take more than 500ms to process everything in the write queue
            {
              fc::microseconds write_lock_held_duration = fc::time_point::now() - write_lock_acquired_time;
              if (write_lock_held_duration > fc::milliseconds(write_lock_hold_time))
              {
                wlog("Stopped processing write_queue before empty because we exceeded ${write_lock_hold_time}ms, "
                    "held lock for ${write_lock_held_duration}μs",
                    (write_lock_hold_time)
                    ("write_lock_held_duration", write_lock_held_duration.count()));

                if ( is_running() ) {
                  break;
                }
                /**
                * Ensure the loop empties the write queue when the application is closing.
                * When the application is closing, the P2P plugin relies on the shutdown_mgr::wait
                * method to process all the blocks it pushed to the queue before exiting. In cases
                * where processing a block takes too long and the application is in the process of
                * closing (not running), the remaining blocks in the queue must be processed by the
                * 'req_visitor' in order to release the pending shutdown_mgr::wait. The blocks themselves
                * will not be modified, as the visitor's 'visit' method immediately returns when a
                * closing is pending. Instead, only the promises associated with the blocks will be triggered,
                * advancing the shutdown_mgr::wait process. Without this mechanism, the application would hang
                * while waiting for processing all the blocks in the queue.
                */
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

        if (is_syncing && get_time_gap_to_live_sync( head_block_time ).count() < 0) //we're syncing, see if we are close enough to move to live sync
        {
          is_syncing = false;
          db.notify_end_of_syncing();
          default_block_writer->set_is_at_live_sync();
          theApp.notify_status("entering live mode");
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
      if( exit_at_block > 0 && exit_at_block == last_block_number )
      {
        ilog("Exiting application on user request, because requested block ${exit_at_block} reached (--exit-at-block).", (exit_at_block));
        theApp.kill();
      }
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
  theApp.notify_status("finished syncing");
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

bool chain_plugin_impl::start_replay_processing( 
  std::shared_ptr< block_write_i > reindex_block_writer,
  hive::chain::blockchain_worker_thread_pool& thread_pool )
{
  db.set_block_writer( reindex_block_writer.get() );

  BOOST_SCOPE_EXIT(this_) {
    this_->db.set_block_writer( this_->default_block_writer.get() );
  } BOOST_SCOPE_EXIT_END

  theApp.notify_status("replaying");
  bool replay_is_last_operation = 
    replay_blockchain( reindex_block_writer->get_block_reader(), thread_pool );
  theApp.notify_status("finished replaying");

  if( replay_is_last_operation )
  {
    if( !theApp.is_interrupt_request() )
    {
      ilog("Generating artificial interrupt request...");

      /*
        Triggering artificial signal.
        Whole application should be closed in identical way, as if it was closed by user.
        This case occurs only when `exit-after-replay` switch is used.
      */
      theApp.generate_interrupt_request();
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
  db.set_block_writer( default_block_writer.get() );

  if( statsd_on_replay )
  {
    auto statsd = theApp.find_plugin< hive::plugins::statsd::statsd_plugin >();
    if( statsd != nullptr )
    {
      statsd->start_logging();
    }
  }

  ilog( "Starting chain with shared_file_size: ${n} bytes", ("n", shared_memory_size) );

  if(resync)
  {
    wlog("resync requested: deleting block log and shared memory");
    db.wipe( shared_memory_dir );
    default_block_writer->close();
    block_storage->close_storage();
    block_storage->wipe_storage_files( theApp.data_dir() / "blockchain" );
  }

  db.set_flush_interval( flush_interval );
  add_checkpoints( loaded_checkpoints, thread_pool  );
  db.set_require_locking( check_locks );

  const auto& abstract_index_cntr = db.get_abstract_index_cntr();

  get_indexes_memory_details = [ this, &abstract_index_cntr ]
    (index_memory_details_cntr_t& index_memory_details_cntr)
  {
    for (auto idx : abstract_index_cntr)
    {
      auto info = idx->get_statistics();
      index_memory_details_cntr.emplace_back(std::move(info._value_type_name), info._item_count,
        info._item_sizeof, info._item_additional_allocation, info._additional_container_allocation);
    }
  };

  fc::variant database_config;

  db_open_args.data_dir = theApp.data_dir() / "blockchain";
  db_open_args.shared_mem_dir = shared_memory_dir;
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
  db_open_args.load_snapshot = load_snapshot;

  bl_open_args.data_dir = db_open_args.data_dir;
  bl_open_args.enable_block_log_compression = enable_block_log_compression;
  bl_open_args.enable_block_log_auto_fixing = enable_block_log_auto_fixing;
  bl_open_args.block_log_compression_level = block_log_compression_level;
}

bool chain_plugin_impl::check_data_consistency( const block_read_i& block_reader )
{
  uint64_t head_block_num_origin = 0;
  uint64_t head_block_num_state = 0;

  auto _is_reindex_complete =
    is_reindex_complete( &head_block_num_origin, &head_block_num_state, block_reader );

  if( !_is_reindex_complete )
  {
    if( head_block_num_state > head_block_num_origin )
    {
      theApp.generate_interrupt_request();
      return false;
    }
    if( db.get_snapshot_loaded() )
    {
      wlog( "Replaying has to be forced, after snapshot's loading. { \"block_log-head\": ${b1}, \"state-head\": ${b2} }", ( "b1", head_block_num_origin )( "b2", head_block_num_state ) );
    }
    else
    {
      wlog( "Replaying is not finished. Synchronization is not allowed. { \"block_log-head\": ${b1}, \"state-head\": ${b2} }", ( "b1", head_block_num_origin )( "b2", head_block_num_state ) );
      theApp.generate_interrupt_request();
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

    db.with_write_lock([&]()
    {
      block_storage->open_and_init( bl_open_args, true/*read_only*/ );
    });
    default_block_writer->open();
    db.open( db_open_args );

    if( dump_memory_details )
    {
      setup_benchmark_dumper();
      dumper.dump( true, get_indexes_memory_details );
    }
  }
  catch( const fc::exception& e )
  {
    /// This is a hack - seems blockchain_worker_thread_pool is completely out of control in the errorneous cases and can lead to 2nd level crash
    thread_pool.shutdown();

    wlog( "Error opening database or block log. If the binary or configuration has changed, replay the blockchain explicitly using `--force-replay`." );
    wlog( " Error: ${e}", ("e", e) );
    theApp.notify_status("exitting with open database error");

    /// this exit shall be eliminated and exception caught inside application::startup, then force app exit with given code (but without calling exit function).
    exit(EXIT_FAILURE);
  }
}

void chain_plugin_impl::push_transaction( const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip )
{
  const signed_transaction& trx = full_transaction->get_transaction(); // just for the rethrow
  try
  {
    if( not db.is_fast_confirm_transaction( full_transaction ) )
    {
      db.process_non_fast_confirm_transaction( full_transaction, skip );
      return;
    }

    auto sf = [&]( std::shared_ptr<full_block_type> new_head_block, uint32_t old_last_irreversible )-> std::optional< uint32_t > {
      try
      {
        hive::chain::detail::without_pending_transactions( db, existing_block_flow_control( new_head_block ),
          std::move( db._pending_tx ), [&]() {
            try
            {
              const block_id_type& new_head_block_id = new_head_block->get_block_id();
              uint32_t new_head_block_num = new_head_block->get_block_num();

              uint32_t skip = db.get_node_skip_flags();
              default_block_writer->switch_forks( 
                new_head_block_id,
                new_head_block_num,
                skip,
                nullptr /*pushed_block_ctrl*/,
                db.head_block_id(), db.head_block_num(),
                [&] ( const std::shared_ptr< full_block_type >& fb,
                      uint32_t skip, const block_flow_control* block_ctrl )
                  { db.apply_block_extended(fb,skip,block_ctrl); },
                [&] ( const block_id_type end_block ) -> uint32_t
                  { return db.pop_block_extended( end_block ); }
                );
              theApp.notify("switching forks", "id", new_head_block_id.str(), "num", new_head_block_num);

              // when we switch forks, irreversibility will be re-evaluated at the end of every block pushed
              // on the new fork, so we don't need to mark the block as irreversible here
              return std::optional< uint32_t >( old_last_irreversible );
            }
            FC_CAPTURE_AND_RETHROW()
        });
      }
      catch( const fc::exception& )
      {
        return std::optional< uint32_t >( old_last_irreversible );
      }
      return std::optional< uint32_t >();
    }; // end sf lambda

    // fast-confirm transactions are just processed in memory, they're not added to the blockchain
    db.process_fast_confirm_transaction( full_transaction, sf );
  }
  FC_CAPTURE_AND_RETHROW((trx))
}

void chain_plugin_impl::add_checkpoints( const flat_map< uint32_t, block_id_type >& checkpts, hive::chain::blockchain_worker_thread_pool& thread_pool )
{
  for( const auto& i : checkpts )
    checkpoints[i.first] = i.second;
  if (!checkpoints.empty())
    thread_pool.set_last_checkpoint(checkpoints.rbegin()->first);
}

bool chain_plugin_impl::before_last_checkpoint()const
{
  return (checkpoints.size() > 0) && (checkpoints.rbegin()->first >= db.head_block_num());
}

void chain_plugin_impl::finish_request()
{
  std::unique_lock<std::mutex> _guard( finish.mtx );
  finish.cv.wait( _guard, [this](){ return finish.status.load(); } );
}

bool chain_plugin_impl::push_block( const block_flow_control& block_ctrl, uint32_t skip )
{
  const std::shared_ptr<full_block_type>& full_block = block_ctrl.get_full_block();
  const signed_block& new_block = full_block->get_block();

  uint32_t block_num = full_block->get_block_num();
  if( checkpoints.size() && checkpoints.rbegin()->second != block_id_type() )
  {
    auto itr = checkpoints.find( block_num );
    if( itr != checkpoints.end() )
      FC_ASSERT(full_block->get_block_id() == itr->second, "Block did not match checkpoint", ("checkpoint", *itr)("block_id", full_block->get_block_id()));

    if( checkpoints.rbegin()->first >= block_num )
    {
      skip = database::skip_witness_signature
          | database::skip_transaction_signatures
          | database::skip_transaction_dupe_check
          /*| skip_fork_db Fork db cannot be skipped or else blocks will not be written out to block log */
          | database::skip_block_size_check
          | database::skip_tapos_check
          | database::skip_authority_check
          /* | skip_merkle_check While blockchain is being downloaded, txs need to be validated against block headers */
          | database::skip_undo_history_check
          | database::skip_witness_schedule_check
          | database::skip_validate
          | database::skip_validate_invariants
          ;
      if (!last_pushed_block_was_before_checkpoint)
      {
        // log something to let the node operator know that checkpoints are in force
        ilog("checkpoints enabled, doing reduced validation until final checkpoint, block ${block_num}, id ${block_id}",
             ("block_num", checkpoints.rbegin()->first)("block_id", checkpoints.rbegin()->second));
        last_pushed_block_was_before_checkpoint = true;
      }
    }
    else if (last_pushed_block_was_before_checkpoint)
    {
      ilog("final checkpoint reached, resuming normal block validation");
      last_pushed_block_was_before_checkpoint = false;
    }
  }

  bool result;
  hive::chain::detail::with_skip_flags( db, skip, [&]()
  {
    hive::chain::detail::without_pending_transactions( db, block_ctrl, std::move(db._pending_tx), [&]()
    {
      try
      {
        result = _push_block( block_ctrl );
        block_ctrl.on_end_of_apply_block();
        db.notify_finish_push_block( full_block );
      }
      FC_CAPTURE_AND_RETHROW((new_block))

      db.check_free_memory( false, full_block->get_block_num() );
    });
  });

  //fc::time_point end_time = fc::time_point::now();
  //fc::microseconds dt = end_time - begin_time;
  //if( ( new_block.block_num() % 10000 ) == 0 )
  //   ilog( "push_block ${b} took ${t} microseconds", ("b", new_block.block_num())("t", dt.count()) );
  return result;
}

bool chain_plugin_impl::_push_block(const block_flow_control& block_ctrl)
{ try {
  const std::shared_ptr<full_block_type>& full_block = block_ctrl.get_full_block();
  const uint32_t skip = db.get_node_skip_flags();

  return default_block_writer->push_block(
    full_block,
    block_ctrl,
    db.head_block_num(),
    db.head_block_id(),
    skip,
    [&] ( const std::shared_ptr< full_block_type >& fb,
          uint32_t skip, const block_flow_control* block_ctrl )
      { db.apply_block_extended(fb,skip,block_ctrl); },
    [&] ( const block_id_type end_block ) -> uint32_t
      { return db.pop_block_extended( end_block ); }
    );
} FC_CAPTURE_AND_RETHROW() }

uint32_t chain_plugin_impl::reindex( const open_args& args, const block_read_i& block_reader, hive::chain::blockchain_worker_thread_pool& thread_pool )
{
  reindex_notification note( args );

  BOOST_SCOPE_EXIT(this_,&note) {
    HIVE_TRY_NOTIFY(this_->db._post_reindex_signal, note);
  } BOOST_SCOPE_EXIT_END

  try
  {
    ilog( "Reindexing Blockchain" );


    if( theApp.is_interrupt_request() )
      return 0;

    uint32_t _head_block_num = db.head_block_num();

    std::shared_ptr<full_block_type> _head = block_reader.head_block();
    if( _head )
    {
      if( args.stop_replay_at == 0 )
        note.max_block_number = _head->get_block_num();
      else
        note.max_block_number = std::min( args.stop_replay_at, _head->get_block_num() );
    }
    else
      note.max_block_number = 0;//anyway later an assert is triggered

    note.force_replay = args.force_replay || _head_block_num == 0;
    note.validate_during_replay = args.validate_during_replay;

    HIVE_TRY_NOTIFY(db._pre_reindex_signal, note);

    default_block_writer->on_reindex_start();

    auto start_time = fc::time_point::now();
    HIVE_ASSERT( _head, block_log_exception, "No blocks in block log. Cannot reindex an empty chain." );

    ilog( "Replaying blocks..." );

    db.with_write_lock( [&]()
    {
      std::shared_ptr<full_block_type> start_block;

      bool replay_required = true;

      if( _head_block_num > 0 )
      {
        if( args.stop_replay_at == 0 || args.stop_replay_at > _head_block_num )
          start_block = block_reader.read_block_by_num( _head_block_num + 1 );

        if( !start_block )
        {
          start_block = block_reader.read_block_by_num( _head_block_num );
          FC_ASSERT( start_block, "Head block number for state: ${h} but for `block_log` this block doesn't exist", ( "h", _head_block_num ) );

          replay_required = false;
        }
      }
      else
      {
        start_block = block_reader.read_block_by_num( 1 );
      }

      if( replay_required )
      {
        auto _last_block_number = start_block->get_block_num();
        if( _last_block_number && !args.force_replay )
          ilog("Resume of replaying. Last applied block: ${n}", ( "n", _last_block_number - 1 ) );

        note.last_block_number = reindex_internal( args, start_block, block_reader, thread_pool );
      }
      else
      {
        note.last_block_number = start_block->get_block_num();
      }

      db.set_revision( db.head_block_num() );

      //get_index< account_index >().indices().print_stats();
    });

    FC_ASSERT( block_reader.head_block()->get_block_num(), "this should never happen" );
    default_block_writer->on_reindex_end( block_reader.head_block() );

    auto end_time = fc::time_point::now();
    ilog("Done reindexing, elapsed time: ${elapsed_time} sec",
         ("elapsed_time", double((end_time - start_time).count()) / 1000000.0));

    note.reindex_success = true;

    return note.last_block_number;
  }
  FC_CAPTURE_AND_RETHROW( (args.data_dir)(args.shared_mem_dir) )
}

uint32_t chain_plugin_impl::reindex_internal( const open_args& args,
  const std::shared_ptr<full_block_type>& start_block, const block_read_i& block_reader, hive::chain::blockchain_worker_thread_pool& thread_pool )
{
  uint64_t skip_flags = chain::database::skip_validate_invariants | chain::database::skip_block_log;
  if (args.validate_during_replay)
    ulog("Doing full validation during replay at user request");
  else
  {
    skip_flags |= chain::database::skip_witness_signature |
      chain::database::skip_transaction_signatures |
      chain::database::skip_transaction_dupe_check |
      chain::database::skip_tapos_check |
      chain::database::skip_merkle_check |
      chain::database::skip_witness_schedule_check |
      chain::database::skip_authority_check |
      chain::database::skip_validate; /// no need to validate operations
  }

  uint32_t last_block_num = block_reader.head_block()->get_block_num();
  if( args.stop_replay_at > 0 && args.stop_replay_at < last_block_num )
    last_block_num = args.stop_replay_at;

  bool rat = fc::enable_record_assert_trip;
  bool as = fc::enable_assert_stacktrace;
  fc::enable_record_assert_trip = true; //enable detailed backtrace from FC_ASSERT (that should not ever be triggered during replay)
  fc::enable_assert_stacktrace = true;

  BOOST_SCOPE_EXIT( this_ ) { this_->db.clear_tx_status(); } BOOST_SCOPE_EXIT_END
  db.set_tx_status( chain::database::TX_STATUS_BLOCK );

  std::shared_ptr<full_block_type> last_applied_block;
  const auto process_block = [&](const std::shared_ptr<full_block_type>& full_block) {
    const uint32_t current_block_num = full_block->get_block_num();

    if (current_block_num % 100000 == 0)
    {
      std::ostringstream percent_complete_stream;
      percent_complete_stream << std::fixed << std::setprecision(2) << double(current_block_num) * 100 / last_block_num;
      ulog("   ${current_block_num} of ${last_block_num} blocks = ${percent_complete}%   (${free_memory_megabytes}MB shared memory free)",
           ("percent_complete", percent_complete_stream.str())
           (current_block_num)(last_block_num)
           ("free_memory_megabytes", db.get_free_memory() >> 20));
    }

    db.apply_block(full_block, skip_flags);
    last_applied_block = full_block;

    return !theApp.is_interrupt_request();
  };

  const uint32_t start_block_number = start_block->get_block_num();
  process_block(start_block);

  if (start_block_number < last_block_num)
    block_reader.process_blocks(start_block_number + 1, last_block_num, process_block, thread_pool);

  if (theApp.is_interrupt_request())
    ilog("Replaying is interrupted on user request. Last applied: (block number: ${n}, id: ${id})",
         ("n", last_applied_block->get_block_num())("id", last_applied_block->get_block_id()));

  fc::enable_record_assert_trip = rat; //restore flag
  fc::enable_assert_stacktrace = as;

  return last_applied_block->get_block_num();
}

bool chain_plugin_impl::is_reindex_complete( uint64_t* head_block_num_in_blocklog,
  uint64_t* head_block_num_in_db, const block_read_i& block_reader ) const
{
  std::shared_ptr<full_block_type> head = block_reader.head_block();
  uint32_t head_block_num_origin = head ? head->get_block_num() : 0;
  uint32_t head_block_num_state = db.head_block_num();
  ilog( "head_block_num_origin: ${o}, head_block_num_state: ${s}",
        ( "o", head_block_num_origin )( "s", head_block_num_state ) );

  if( head_block_num_in_blocklog ) //if head block number requested
    *head_block_num_in_blocklog = head_block_num_origin;

  if( head_block_num_in_db ) //if head block number in database requested
    *head_block_num_in_db = head_block_num_state;

  if( head_block_num_state > head_block_num_origin )
  elog( "Incorrect number of blocks in `block_log` vs `state`. { \"block_log-head\": ${head_block_num_origin}, \"state-head\": ${head_block_num_state} }",
      ( head_block_num_origin )(head_block_num_state ) );

  return head_block_num_origin == head_block_num_state;
}

bool chain_plugin_impl::replay_blockchain( const block_read_i& block_reader, hive::chain::blockchain_worker_thread_pool& thread_pool )
{
  try
  {
    ilog("Replaying blockchain on user request.");
    uint32_t last_block_number = 0;
    last_block_number = reindex( db_open_args, block_reader, thread_pool );

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
    return theApp.is_interrupt_request()/*user triggered SIGINT/SIGTERM*/ || exit_after_replay/*shutdown node definitely*/;
  } FC_CAPTURE_LOG_AND_RETHROW( () )

  return true;
}

void chain_plugin_impl::process_snapshot()
{
  if( snapshot_provider != nullptr )
    snapshot_provider->process_explicit_snapshot_requests( db_open_args );
}

void chain_plugin_impl::prepare_work( bool started, synchronization_type& on_sync )
{
  if( !started )
  {
    ilog( "Waiting for chain plugin to start" );
    chain_sync_con = on_sync.connect( 0, [this]()
    {
      webserver.start_webserver();
    });
  }
  else
  {
      webserver.start_webserver();
  }
}

void chain_plugin_impl::work( synchronization_type& on_sync )
{
  ilog( "Started on blockchain with ${n} blocks, LIB: ${lb}", ("n", db.head_block_num())("lb", db.get_last_irreversible_block_num()) );

  bool good_bye = false;

  auto right_now_plus_margin = fc::time_point_sec(fc::time_point::now()) + allow_future_time;
  auto state_time = db.head_block_time();
  if (state_time != HIVE_GENESIS_TIME && // GENESIS (default value) is obiously allowed to be in future
      state_time > right_now_plus_margin) // also cut some sclack for compatibility with check_time_in_block
  {
    ilog( "Shutting down node due to time travel detected - state is from future (${state_time} vs ${right_now_plus_margin}).",
          (state_time)(right_now_plus_margin) );
    good_bye = true;
  }

  const bool exit_at_block_reached = exit_at_block > 0 && exit_at_block == db.head_block_num();
  if (this->exit_before_sync || exit_at_block_reached)
  {
    ilog("Shutting down node without performing any action on user request");
    good_bye = true;
  }

  if (good_bye)
  {
    theApp.kill();
    return;
  }
  else
  {
    block_storage->reopen_for_writing();
    ilog( "Started on blockchain with ${n} blocks", ("n", db.head_block_num()) );
  }

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
        auto info = idx->get_statistics();
        database_object_sizeof_cntr.emplace_back(std::move(info._value_type_name), info._item_sizeof);
      }
    };
    this->dumper.initialize( get_database_objects_sizeofs, (this->dump_memory_details ? BENCHMARK_FILE_NAME : "") );
  }
}

void chain_plugin_impl_deleter::operator()( chain_plugin_impl* impl ) const
{
  delete impl;
}

} // detail


chain_plugin::chain_plugin(){}
chain_plugin::~chain_plugin(){}

database& chain_plugin::db() { return my->db; }
const hive::chain::database& chain_plugin::db() const { return my->db; }

const block_read_i& chain_plugin::block_reader() const
{
  // When other plugins are able to call this method, replay is complete (if required)
  // and default syncing block writer is being used.
  return my->default_block_writer->get_block_reader();
}

fc::microseconds chain_plugin::get_time_gap_to_live_sync( const fc::time_point_sec& head_block_time )
{
  return my->get_time_gap_to_live_sync( head_block_time );
}

hive::chain::blockchain_worker_thread_pool& chain_plugin::get_thread_pool()
{
  return my->thread_pool;
}

bfs::path chain_plugin::state_storage_dir() const
{
  return my->shared_memory_dir;
}

void chain_plugin::set_program_options(options_description& cli, options_description& cfg)
{
#ifdef USE_ALTERNATE_CHAIN_ID
  using hive::protocol::testnet_blockchain_configuration::configuration_data;
  /// Default initminer priv. key in non-mainnet builds.
  auto default_skeleton_privkey = configuration_data.get_default_initminer_private_key().key_to_wif();
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
      ("enable-block-log-auto-fixing", boost::program_options::value<bool>()->default_value(true), "If enabled, corrupted block_log will try to fix itself automatically." )
      ("block-log-compression-level", bpo::value<int>()->default_value(15), "Block log zstd compression level 0 (fast, low compression) - 22 (slow, high compression)" )
      ("blockchain-thread-pool-size", bpo::value<uint32_t>()->default_value(8)->value_name("size"), "Number of worker threads used to pre-validate transactions and blocks")
      ("block-stats-report-type", bpo::value<string>()->default_value("FULL"), "Level of detail of block stat reports: NONE, MINIMAL, REGULAR, FULL. Default FULL (recommended for API nodes)." )
      ("block-stats-report-output", bpo::value<string>()->default_value("ILOG"), "Where to put block stat reports: DLOG, ILOG, NOTIFY, LOG_NOTIFY. Default ILOG." )
#ifdef USE_ALTERNATE_CHAIN_ID
      ("alternate-chain-spec", boost::program_options::value<string>(), "Filepath for the alternate chain specification in JSON format")
#endif
      ("rc-stats-report-type", bpo::value<string>()->default_value( "REGULAR" ), "Level of detail of daily RC stat reports: NONE, MINIMAL, REGULAR, FULL. Default REGULAR." )
      ("rc-stats-report-output", bpo::value<string>()->default_value( "ILOG" ), "Where to put daily RC stat reports: DLOG, ILOG, NOTIFY, LOG_NOTIFY. Default ILOG." )
      ("block-log-split", bpo::value<int>()->default_value( -1 ), "Whether the block log should be single file (-1), split into files each containing 1M blocks (0), or split & keeping only N latest files, i.e. N million latest blocks (N). Default -1." )
      ;
  cli.add_options()
      ("replay-blockchain", bpo::bool_switch()->default_value(false), "clear chain database and replay all blocks" )
      ("resync-blockchain", bpo::bool_switch()->default_value(false), "clear chain database and block log" )
      ("stop-replay-at-block", bpo::value<uint32_t>(), "[ DEPRECATED ] Stop replay after reaching given block number")
      ("stop-at-block", bpo::value<uint32_t>(), "Stop after reaching given block number")
      ("exit-at-block", bpo::value<uint32_t>(), "Same as --stop-at-block, but also exit the application")
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

void chain_plugin::plugin_initialize(const variables_map& options)
{ try {

  my.reset( new detail::chain_plugin_impl( get_app() ) );

  my->block_storage = block_storage_i::create_storage( options.at( "block-log-split" ).as< int >(), get_app(), my->thread_pool );
  my->default_block_writer = 
    std::make_unique< sync_block_writer >( *( my->block_storage.get() ), my->db, get_app() );


  get_app().setup_notifications(options);
  my->shared_memory_dir = get_app().data_dir() / "blockchain";

  if( options.count("shared-file-dir") )
  {
    auto sfd = options.at("shared-file-dir").as<bfs::path>();
    if(sfd.is_relative())
      my->shared_memory_dir = get_app().data_dir() / sfd;
    else
      my->shared_memory_dir = sfd;
  }

  my->shared_memory_size = fc::parse_size( options.at( "shared-file-size" ).as< string >() );

  if( options.count( "shared-file-full-threshold" ) )
    my->shared_file_full_threshold = options.at( "shared-file-full-threshold" ).as< uint16_t >();

  if( options.count( "shared-file-scale-rate" ) )
    my->shared_file_scale_rate = options.at( "shared-file-scale-rate" ).as< uint16_t >();

  my->force_replay        = options.count( "force-replay" ) ? options.at( "force-replay" ).as<bool>() : false;
  my->validate_during_replay =
    options.count( "validate-during-replay" ) ? options.at( "validate-during-replay" ).as<bool>() : false;
  my->replay              = options.at( "replay-blockchain").as<bool>() || my->force_replay;
  my->resync              = options.at( "resync-blockchain").as<bool>();
  my->stop_replay_at      = options.count( "stop-replay-at-block" ) ? options.at( "stop-replay-at-block" ).as<uint32_t>() : 0;
  my->stop_at_block       = options.count( "stop-at-block" ) ? options.at( "stop-at-block" ).as<uint32_t>() : 0;
  my->exit_at_block       = options.count( "exit-at-block" ) ? options.at( "exit-at-block" ).as<uint32_t>() : 0;
  my->exit_before_sync    = options.count( "exit-before-sync" ) ? options.at( "exit-before-sync" ).as<bool>() : false;
  my->benchmark_interval  =
    options.count( "set-benchmark-interval" ) ? options.at( "set-benchmark-interval" ).as<uint32_t>() : 0;
  my->check_locks         = options.at( "check-locks" ).as< bool >();
  my->validate_invariants = options.at( "validate-database-invariants" ).as<bool>();
  my->dump_memory_details = options.at( "dump-memory-details" ).as<bool>();
  my->enable_block_log_compression = options.at( "enable-block-log-compression" ).as<bool>();
  my->enable_block_log_auto_fixing = options.at( "enable-block-log-auto-fixing" ).as<bool>();
  my->block_log_compression_level = options.at( "block-log-compression-level" ).as<int>();

  FC_ASSERT(!(my->stop_replay_at && my->stop_at_block), "--stop-replay-at and --stop-at-block cannot be used together" );
  FC_ASSERT(!(my->stop_replay_at && my->exit_at_block), "--stop-replay-at and --exit-at-block cannot be used together" );
  FC_ASSERT(!(my->exit_at_block && my->stop_at_block), "--exit-at-block and --stop-at-block cannot be used together" );
  if (my->stop_replay_at)
  {
    wlog("flag `--stop-replay-at` is deprecated, please use `--stop-at-block` instead");
  }
  if (my->exit_at_block)
  {
    my->stop_at_block = my->exit_at_block;
  }
  if (my->stop_at_block)
  {
    my->stop_replay_at = my->stop_at_block;
  }

  if (options.count("load-snapshot"))
    my->load_snapshot = true;

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
      auto item = fc::json::from_string(cp, fc::json::format_validation_mode::full).as<std::pair<uint32_t,block_id_type>>();
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

      fc::optional<fc::ecc::private_key> skeleton_key = fc::ecc::private_key::wif_to_key(wif_key);
      FC_ASSERT(skeleton_key.valid(), "unable to parse passed skeletpn key: ${sk}", ("sk", wif_key));

      configuration_data.set_skeleton_key(*skeleton_key);
    }

  }

  if( options.count( "alternate-chain-spec" ) )
  {
    using hive::protocol::testnet_blockchain_configuration::hardfork_schedule_item_t;

    fc::variant alternate_chain_spec = fc::json::from_file( options["alternate-chain-spec"].as< string >(), fc::json::format_validation_mode::full );

    idump((alternate_chain_spec));

    {
      uint64_t hive = 0, hbd = 0, to_vest = 0;
      // default value somewhat close to mainnet - won't have any impact if to_vest remains zero
      // see comment for configuration::set_initial_asset_supply to see why the price looks so weird
      hive::protocol::price vest_price( VEST_asset( 1800 ), HIVE_asset( 1'000 ) );
      bool set = false;

      if( alternate_chain_spec.get_object().contains("init_supply") )
      {
        set = true;
        hive = alternate_chain_spec["init_supply"].as< uint64_t >();
      }
      if( alternate_chain_spec.get_object().contains("hbd_init_supply") )
      {
        set = true;
        hbd = alternate_chain_spec["hbd_init_supply"].as< uint64_t >();
      }
      if( alternate_chain_spec.get_object().contains( "initial_vesting" ) )
      {
        set = true;

        fc::variant initial_vesting = alternate_chain_spec[ "initial_vesting" ];
        FC_ASSERT( initial_vesting.get_object().contains( "hive_amount" ) );
        FC_ASSERT( initial_vesting.get_object().contains( "vests_per_hive" ) );

        to_vest = initial_vesting[ "hive_amount" ].as< uint64_t >();
        uint64_t vests_per_hive = initial_vesting[ "vests_per_hive" ].as< uint64_t >();

        vest_price = hive::protocol::price(
          hive::protocol::VEST_asset( vests_per_hive ),
          hive::protocol::HIVE_asset( 1'000 ) // see comment for configuration::set_initial_asset_supply
        );
      }

      if( set )
        configuration_data.set_initial_asset_supply( hive, hbd, to_vest, vest_price );
    }

    if( alternate_chain_spec.get_object().contains("min_root_comment_interval") )
      configuration_data.min_root_comment_interval = fc::seconds( alternate_chain_spec["min_root_comment_interval"].as< uint64_t >() );

    if( alternate_chain_spec.get_object().contains("min_reply_interval") )
    {
      configuration_data.min_reply_interval = fc::seconds( alternate_chain_spec["min_reply_interval"].as< uint64_t >() );
      configuration_data.min_reply_interval_hf20 = configuration_data.min_reply_interval;
    }

    if( alternate_chain_spec.get_object().contains("min_comment_edit_interval") )
      configuration_data.min_comment_edit_interval = fc::seconds( alternate_chain_spec["min_comment_edit_interval"].as< uint64_t >() );

    if( alternate_chain_spec.get_object().contains("witness_custom_op_block_limit") )
      configuration_data.witness_custom_op_block_limit = alternate_chain_spec["witness_custom_op_block_limit"].as< uint64_t >();

    if( alternate_chain_spec.get_object().contains( "allow_not_enough_rc" ) )
      configuration_data.allow_not_enough_rc = alternate_chain_spec[ "allow_not_enough_rc" ].as< bool >();

    if( alternate_chain_spec.get_object().contains( "generate_missed_block_operations" ) )
      configuration_data.set_generate_missed_block_operations( alternate_chain_spec[ "generate_missed_block_operations" ].as< bool >() );

    if ( alternate_chain_spec.get_object().contains( "hive_owner_update_limit" ))
      configuration_data.set_hive_owner_update_limit( alternate_chain_spec["hive_owner_update_limit"].as< uint16_t >() );

    if ( alternate_chain_spec.get_object().contains( "hf_21_stall_block" ) )
      configuration_data.hf_21_stall_block = alternate_chain_spec[ "hf_21_stall_block" ].as< uint32_t >();

    std::vector< string > init_witnesses;
    if( alternate_chain_spec.get_object().contains("init_witnesses") )
      init_witnesses = alternate_chain_spec["init_witnesses"].as< std::vector< string > >();

    for( const auto& wit : init_witnesses )
      hive::protocol::validate_account_name( wit );

    std::vector< hardfork_schedule_item_t > result_hardforks;

    FC_ASSERT( alternate_chain_spec.get_object().contains( "hardfork_schedule" ) );
    auto hardfork_schedule = alternate_chain_spec[ "hardfork_schedule" ].as< std::vector< hardfork_schedule_item_t > >();
    // we could allow skipping hardfork_schedule, but we'd need to decide what it means:
    // - "don't change hardfork timestamps at all" would be most natural, but not very useful
    // - "activate all hardforks in first block" is a useful alternative, but that could be achieved by other means,
    //   for example, by allowing -1 as indicator of "latest hardfork"
    FC_ASSERT( hardfork_schedule.size(), "At least one hardfork should be provided in the hardfork_schedule", ("hardfork_schedule", alternate_chain_spec["hardfork_schedule"]) );

    bool is_sorted_hardfork = std::is_sorted(hardfork_schedule.begin(), hardfork_schedule.end(), [&](const auto& __lhs, const auto& __rhs){
      return __lhs.hardfork < __rhs.hardfork;
    });

    bool is_sorted_block_num = std::is_sorted(hardfork_schedule.begin(), hardfork_schedule.end(), [&](const auto& __lhs, const auto& __rhs){
      return __lhs.block_num < __rhs.block_num;
    });

    FC_ASSERT( is_sorted_hardfork && is_sorted_block_num, "hardfork schedule objects should be ascending in hardfork and block numbers", ("hardfork_schedule", alternate_chain_spec["hardfork_schedule"]) );

    for(uint32_t i = 0, j = 0; i < HIVE_NUM_HARDFORKS; ++i)
    {
      FC_ASSERT( hardfork_schedule[j].hardfork > 0, "You cannot specify the hardfork 0 block. Use 'genesis_time' option instead" );
      FC_ASSERT( hardfork_schedule[j].hardfork <= HIVE_NUM_HARDFORKS, "You are not allowed to specify future hardfork times" );

      if( i + 1 > hardfork_schedule[j].hardfork )
        ++j;

      if( j == hardfork_schedule.size() )
        break;

      result_hardforks.emplace_back(hardfork_schedule_item_t{ i + 1, hardfork_schedule[j].block_num });
    }

    // note that we cannot substitute 'now' for missing 'genesis_time', because caller has to give the same configuration
    // every time node is restarted and 'now' would be different each time; that's why this option is required
    FC_ASSERT( alternate_chain_spec.get_object().contains( "genesis_time" ) );
    uint32_t genesis_time = alternate_chain_spec[ "genesis_time" ].as< uint32_t >();

    configuration_data.set_hardfork_schedule( fc::time_point_sec( genesis_time ), result_hardforks );
    configuration_data.set_init_witnesses( init_witnesses );
  }
#endif
  uint32_t blockchain_thread_pool_size = options.at("blockchain-thread-pool-size").as<uint32_t>();
  get_thread_pool().set_thread_pool_size(blockchain_thread_pool_size);

  if (my->validate_during_replay)
    get_thread_pool().set_validate_during_replay();


  block_flow_control::set_auto_report(options.at("block-stats-report-type").as<std::string>(),
                                      options.at("block-stats-report-output").as<std::string>());
  resource_credits::set_auto_report(options.at("rc-stats-report-type").as<std::string>(),
                                    options.at("rc-stats-report-output").as<std::string>());

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

        get_app().notify("hived_benchmark", "multiindex_stats", fc::variant{measure});
      }
    }, *this, 0);
  }
} FC_LOG_AND_RETHROW() }

void chain_plugin::plugin_startup()
{
  ilog("Chain plugin initialization...");

  my->initial_settings();

  ilog("Database opening...");
  my->open();

  ilog("Looking for snapshot processing requests...");
  my->process_snapshot();

  my->prepare_work( get_state() == appbase::abstract_plugin::started, on_sync );

  if( my->replay )
  {
    std::shared_ptr< block_write_i > reindex_block_writer =
      std::make_shared< irreversible_block_writer >( *( my->block_storage.get() ) );
    ilog("Replaying...");
    if( !my->start_replay_processing( reindex_block_writer, get_thread_pool() ) )
    {
      ilog("P2P enabling after replaying...");
      my->work( on_sync );
    }
  }
  else
  {
    ilog("Consistency data checking...");
    if( my->check_data_consistency( my->default_block_writer->get_block_reader() ) )
    {
      if( my->db.get_snapshot_loaded() )
      {
        std::shared_ptr< block_write_i > reindex_block_writer =
          std::make_shared< irreversible_block_writer >( *( my->block_storage.get() ) );
        ilog("Replaying...");
        //Replaying is forced, because after snapshot loading, node should work in synchronization mode.
        if( !my->start_replay_processing( reindex_block_writer, get_thread_pool() ) )
        {
          ilog("P2P enabling after replaying...");
          my->work( on_sync );
        }
      }
      else
      {
        if( my->is_p2p_enabled )
        {
          ilog("P2P enabling...");
          my->work( on_sync );
        }
        else
        {
          ilog("P2P is disabled.");
          my->block_storage->reopen_for_writing();
        }
      }
    }
  }

  ilog("Chain plugin initialization finished...");
  if( get_app().is_interrupt_request() )
    get_app().kill();
}

void chain_plugin::plugin_shutdown()
{
  ilog("closing chain database");
  get_thread_pool().shutdown();
  my->stop_write_processing();
  my->db.close();
  my->default_block_writer->close();
  my->block_storage->close_storage();

  ilog("database closed successfully");
  get_app().notify_status("finished syncing");
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
  const full_block_type* full_block = block_ctrl->get_full_block().get();
  const signed_block& block = full_block->get_block();
  if( my->sync_progress )
  {
    my->sync_progress->total_block_size += full_block->get_uncompressed_block_size();
    my->sync_progress->total_tx_count += block.transactions.size();
  }
  if (currently_syncing && block.block_num() % 10000 == 0)
  {
    fc::time_point now = fc::time_point::now();
    if (my->sync_progress)
    {
      fc::microseconds duration = now - my->sync_progress->last_myriad_time;
      float microseconds_per_block = (float)duration.count() / 10000.f;
      std::ostringstream microseconds_per_block_stream;
      microseconds_per_block_stream << std::setprecision(2) << std::fixed << microseconds_per_block;
      ilog("Syncing Blockchain --- Got block: #${n} time: ${t} producer: ${p} --- ${microseconds_per_block}µs/block, avg. size ${s}, avg. tx count ${c}",
           ("t", block.timestamp)("n", block.block_num())("p", block.witness)("microseconds_per_block", microseconds_per_block_stream.str())
           ("s", (my->sync_progress->total_block_size+5000)/10000)("c", (my->sync_progress->total_tx_count+5000)/10000));
    }
    else
    {
      ilog("Syncing Blockchain --- Got block: #${n} time: ${t} producer: ${p}",
           ("t", block.timestamp)("n", block.block_num())("p", block.witness));
    }
#define DISPLAY_SYNC_SPEED
#ifdef DISPLAY_SYNC_SPEED
    my->sync_progress = detail::chain_plugin_impl::sync_progress_data{ now, 0, 0 };
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
  static std::atomic_int call_count = 0;
  call_count++;
  BOOST_SCOPE_EXIT(&call_count) {
    --call_count;
    fc_dlog(fc::logger::get("chainlock"), "<-- accept_transaction_calls_in_progress: ${call_count}", (call_count.load()));
  } BOOST_SCOPE_EXIT_END

  if (lock == lock_type::boost)
  {
    fc_dlog(fc::logger::get("chainlock"), "--> boost accept_transaction_calls_in_progress: ${call_count}", (call_count.load()));
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
    fc_dlog(fc::logger::get("chainlock"), "--> fc accept_transaction_calls_in_progress: ${call_count}", (call_count.load()));
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
  get_thread_pool().enqueue_work(result, blockchain_worker_thread_pool::data_source_type::standalone_transaction_received_from_api);
  try
  {
    accept_transaction(result, lock);
  }
  catch (const hive::protocol::transaction_auth_exception&)
  {
    on_full_trx( true );
    result = full_transaction_type::create_from_signed_transaction( trx, hive::protocol::pack_type::legacy, true /* cache this transaction */);
    on_full_trx( false );
    get_thread_pool().enqueue_work(result, blockchain_worker_thread_pool::data_source_type::standalone_transaction_received_from_api);
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

void chain_plugin::push_transaction( const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip )
{
  my->push_transaction( full_transaction, skip );
}

/**
  * Push block "may fail" in which case every partial change is unwound.  After
  * push block is successful the block is appended to the chain database on disk.
  *
  * @return true if we switched forks as a result of this push.
  */
bool chain_plugin::push_block( const block_flow_control& block_ctrl, uint32_t skip )
{
  return my->push_block( block_ctrl, skip );
}

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

void chain_plugin::disable_p2p() const
{
  my->is_p2p_enabled = false;
}

void chain_plugin::finish_request()
{
  my->finish_request();
}
} } } // namespace hive::plugis::chain::chain_apis

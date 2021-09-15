#include <hive/plugins/sql_serializer/sql_serializer_plugin.hpp>

#include <hive/plugins/sql_serializer/cached_data.h>
#include <hive/plugins/sql_serializer/reindex_data_dumper.h>
#include <hive/plugins/sql_serializer/livesync_data_dumper.h>

#include <hive/plugins/sql_serializer/data_processor.hpp>

#include <hive/plugins/sql_serializer/sql_serializer_objects.hpp>

#include <hive/chain/util/impacted.hpp>

#include <hive/protocol/config.hpp>
#include <hive/protocol/operations.hpp>

#include <hive/utilities/plugin_utilities.hpp>

#include <fc/io/json.hpp>
#include <fc/io/sstream.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/utf8.hpp>

#include <boost/filesystem.hpp>

#include <condition_variable>
#include <map>
#include <sstream>
#include <string>
#include <vector>


namespace hive
{
using chain::block_notification;
using chain::operation_notification;
using chain::reindex_notification;

  namespace plugins
  {
    namespace sql_serializer
    {

    inline std::string get_operation_name(const hive::protocol::operation& op)
    {
      PSQL::name_gathering_visitor v;
      return op.visit(v);
    }

      using num_t = std::atomic<uint64_t>;
      using duration_t = fc::microseconds;
      using stat_time_t = std::atomic<duration_t>;

      struct stat_t
      {
        stat_time_t processing_time{ duration_t{0} };
        num_t count{0};
      };

      struct ext_stat_t : public stat_t
      {
        stat_time_t flush_time{ duration_t{0} };

        void reset()
        {
          processing_time.store( duration_t{0} );
          flush_time.store( duration_t{0} );
          count.store(0);
        }
      };

      struct stats_group
      {
        stat_time_t sending_cache_time{ duration_t{0} };
        uint64_t workers_count{0};
        uint64_t all_created_workers{0};

        ext_stat_t blocks{};
        ext_stat_t transactions{};
        ext_stat_t operations{};

        void reset()
        {
          blocks.reset();
          transactions.reset();
          operations.reset();

          sending_cache_time.store(duration_t{0});
          workers_count = 0;
          all_created_workers = 0;
        }
      };

      inline std::ostream& operator<<(std::ostream& os, const stat_t& obj)
      {
        return os << obj.processing_time.load().count() << " us | count: " << obj.count.load();
      }

      inline std::ostream& operator<<(std::ostream& os, const ext_stat_t& obj)
      {
        return os << "flush time: " << obj.flush_time.load().count() << " us | processing time: " << obj.processing_time.load().count() << " us | count: " << obj.count.load();
      }

      inline std::ostream& operator<<(std::ostream& os, const stats_group& obj)
      {
        #define __shortcut( name ) << #name ": " << obj.name << std::endl
        return os
          << "threads created since last info: " << obj.all_created_workers << std::endl
          << "currently working threads: " << obj.workers_count << std::endl
          __shortcut(blocks)
          __shortcut(transactions)
          __shortcut(operations)
          ;
      }

      using namespace hive::plugins::sql_serializer::PSQL;

      constexpr size_t default_reservation_size{ 16'000u };

      namespace detail
      {

      using data_processing_status = data_processor::data_processing_status;
      using data_chunk_ptr = data_processor::data_chunk_ptr;

      class sql_serializer_plugin_impl final
        {
        public:

          sql_serializer_plugin_impl(const std::string &url, hive::chain::database& _chain_db, const sql_serializer_plugin& _main_plugin) 
            : db_url{url},
              chain_db{_chain_db},
              main_plugin{_main_plugin}
          {
            FC_ASSERT( is_database_correct(), "SQL database is in invalid state" );
            _dumper = std::make_unique< livesync_data_dumper >( url, main_plugin, chain_db );
          }

          ~sql_serializer_plugin_impl()
          {
            ilog("Serializer plugin is closing");

            cleanup_sequence();

            ilog("Serializer plugin has been closed");
          }

          void connect_signals();
          void disconnect_signals();

          void on_pre_reindex(const reindex_notification& note);
          void on_post_reindex(const reindex_notification& note);

          void on_pre_apply_operation(const operation_notification& note);
          void on_pre_apply_block(const block_notification& note);
          void on_post_apply_block(const block_notification& note);

          void handle_transactions(const vector<hive::protocol::signed_transaction>& transactions, const int64_t block_num);

          boost::signals2::connection _on_pre_apply_operation_con;
          boost::signals2::connection _on_pre_apply_block_con;
          boost::signals2::connection _on_post_apply_block_con;
          boost::signals2::connection _on_starting_reindex;
          boost::signals2::connection _on_finished_reindex;

          std::unique_ptr< data_dumper > _dumper;

          std::string db_url;
          hive::chain::database& chain_db;
          const sql_serializer_plugin& main_plugin;

          uint32_t _last_block_num = 0;

          uint32_t last_skipped_block = 0;
          uint32_t psql_block_number = 0;
          uint32_t psql_index_threshold = 0;
          uint32_t psql_transactions_threads_number = 2;
          uint32_t psql_operations_threads_number = 5;
          uint32_t head_block_number = 0;

          int64_t block_vops = 0;
          int64_t op_sequence_id = 0; 

          cached_containter_t currently_caching_data;
          stats_group current_stats;

          void log_statistics()
          {
            std::cout << current_stats;
            current_stats.reset();
          }

          auto get_switch_indexes_function( const std::string& query, bool mode, const std::string& objects_name ) {
              auto function = [query, &objects_name, mode](const data_chunk_ptr&, transaction& tx) -> data_processing_status
              {
                ilog("Attempting to execute query: `${query}`...", ("query", query ) );
                const auto start_time = fc::time_point::now();
                tx.exec( query );
                ilog(
                  "${mode} of ${mod_type} done in ${time} ms",
                  ("mode", (mode ? "Creating" : "Saving and dropping")) ("mod_type", objects_name) ("time", (fc::time_point::now() - start_time).count() / 1000.0 )
                  );
                return data_processing_status();
              };

              return function;
          }

          std::unique_ptr<queries_commit_data_processor> switch_db_items(bool mode, const std::string& sql_function_call, const std::string& objects_name ) const
          {
            ilog("${mode} ${objects_name}...", ("objects_name", objects_name )("mode", ( mode ? "Creating" : "Dropping" ) ) );

            std::string query = std::string("SELECT ") + sql_function_call + ";";
            std::string description = "Query processor: `" + query + "'";
            auto processor=std::make_unique< queries_commit_data_processor >(db_url, description, [query, &objects_name, mode, description](const data_chunk_ptr&, transaction& tx) -> data_processing_status
                          {
                            ilog("Attempting to execute query: `${query}`...", ("query", query ) );
                            const auto start_time = fc::time_point::now();
                            tx.exec( query );
                            ilog(
                              "${d} ${mode} of ${mod_type} done in ${time} ms",
                              ("d", description)("mode", (mode ? "Creating" : "Saving and dropping")) ("mod_type", objects_name) ("time", (fc::time_point::now() - start_time).count() / 1000.0 )
                            );
                            ilog("The ${objects_name} have been ${mode}...", ("objects_name", objects_name )("mode", ( mode ? "created" : "dropped" ) ) );
                            return data_processing_status();
                          } , nullptr);

            processor->trigger(data_processor::data_chunk_ptr(), 0);
            return processor;
          }

          void switch_db_items( bool create ) const
          {
            if( psql_block_number == 0 || ( psql_block_number + psql_index_threshold <= head_block_number ) )
            {
              ilog("Switching indexes and constraints is allowed: psql block number: `${pbn}` psql index threshold: `${pit}` head block number: `${hbn}` ...",
              ("pbn", psql_block_number)("pit", psql_index_threshold)("hbn", head_block_number));

              if(create)
              {
                auto restore_blocks_idxs = switch_db_items( create, "hive.restore_indexes_constraints( 'hive.blocks' )", "enable indexes" );
                auto restore_irreversible_idxs = switch_db_items( create, "hive.restore_indexes_constraints( 'hive.irreversible_data' )", "enable indexes" );
                auto restore_transactions_idxs = switch_db_items( create, "hive.restore_indexes_constraints( 'hive.transactions' )", "enable indexes" );
                auto restore_transactions_sigs_idxs = switch_db_items( create, "hive.restore_indexes_constraints( 'hive.transactions_multisig' )", "enable indexes" );
                auto restore_operations_idxs = switch_db_items( create, "hive.restore_indexes_constraints( 'hive.operations' )", "enable indexes" );
                restore_blocks_idxs->join();
                restore_irreversible_idxs->join();
                restore_transactions_idxs->join();
                restore_transactions_sigs_idxs->join();
                restore_operations_idxs->join();

                ilog( "All irreversible blocks tables indexes are re-created" );

                auto restore_blocks_fks = switch_db_items( create, "hive.restore_foreign_keys( 'hive.blocks' )", "enable indexes" );
                auto restore_irreversible_fks = switch_db_items( create, "hive.restore_foreign_keys( 'hive.irreversible_data' )", "enable indexes" );
                auto restore_transactions_fks = switch_db_items( create, "hive.restore_foreign_keys( 'hive.transactions' )", "enable indexes" );
                auto restore_transactions_sigs_fks = switch_db_items( create, "hive.restore_foreign_keys( 'hive.transactions_multisig' )", "enable indexes" );
                auto restore_operations_fks = switch_db_items( create, "hive.restore_foreign_keys( 'hive.operations' )", "enable indexes" );
                restore_blocks_fks->join();
                restore_irreversible_fks->join();
                restore_transactions_fks->join();
                restore_transactions_sigs_fks->join();
                restore_operations_fks->join();

                ilog( "All irreversible blocks tables foreighn keys are re-created" );
              }
              else
              {
                auto processor = switch_db_items(create, "hive.disable_indexes_of_irreversible()", "disable indexes" );
                processor->join();
                ilog( "All irreversible blocks tables indexes and foreighn keys are dropped" );
              }
            }
            else
            {
              ilog("Switching indexes and constraints isn't allowed: psql block number: `${pbn}` psql index threshold: `${pit}` head block number: `${hbn}` ...",
              ("pbn", psql_block_number)("pit", psql_index_threshold)("hbn", head_block_number));
            }
          }

          void init_database(bool freshDb, uint32_t max_block_number )
          {
            head_block_number = max_block_number;

            load_initial_db_data();

            if(freshDb)
            {
              auto get_type_definitions = [](const data_processor::data_chunk_ptr& dataPtr, transaction& tx){
                auto types = PSQL::get_all_type_definitions();
                if ( types.empty() )
                  return data_processing_status();;

                tx.exec( types );
                return data_processing_status();
              };
              queries_commit_data_processor processor( db_url, "Get type definitions", get_type_definitions, nullptr );
              processor.trigger( nullptr, 0 );
              processor.join();
            }

            switch_db_items( false/*mode*/ );
          }

          bool is_database_correct() {
            ilog( "Checking correctness of database..." );

            bool is_extension_created = false;
            queries_commit_data_processor db_checker(
                  db_url
                , "Check correctness"
                , [&is_extension_created](const data_chunk_ptr&, transaction& tx) -> data_processing_status {
                    pqxx::result data = tx.exec("select 1 as _result from pg_extension where extname='hive_fork_manager';");
                    is_extension_created = !data.empty();
                    return data_processing_status();
                }
              , nullptr
              );

            db_checker.trigger(data_processor::data_chunk_ptr(), 0);
            db_checker.join();

            return is_extension_created;
          }

          void load_initial_db_data()
          {
            ilog("Loading operation's last id ...");

            op_sequence_id = 0;
            psql_block_number = 0;

            queries_commit_data_processor block_loader(db_url, "Block loader",
                                                       [this](const data_chunk_ptr&, transaction& tx) -> data_processing_status
              {
                pqxx::result data = tx.exec("SELECT hb.num AS _max_block FROM hive.blocks hb ORDER BY hb.num DESC LIMIT 1;");
                if( !data.empty() )
                {
                  FC_ASSERT( data.size() == 1 );
                  const auto& record = data[0];
                  psql_block_number = record["_max_block"].as<uint64_t>();
                  _last_block_num = psql_block_number;
                }
                return data_processing_status();
              }
              , nullptr
            );

            block_loader.trigger(data_processor::data_chunk_ptr(), 0);

            queries_commit_data_processor sequence_loader(db_url, "Sequence loader",
                                                          [this](const data_chunk_ptr&, transaction& tx) -> data_processing_status
              {
                pqxx::result data = tx.exec("SELECT ho.id AS _max FROM hive.operations ho ORDER BY ho.id DESC LIMIT 1;");
                if( !data.empty() )
                {
                  FC_ASSERT( data.size() == 1 );
                  const auto& record = data[0];
                  op_sequence_id = record["_max"].as<int64_t>();
                }
                return data_processing_status();
              }
              , nullptr
            );

            sequence_loader.trigger(data_processor::data_chunk_ptr(), 0);

            sequence_loader.join();
            block_loader.join();

            ilog("Next operation id: ${s} psql block number: ${pbn}.",
              ("s", op_sequence_id + 1)("pbn", psql_block_number));
          }

          void join_data_processors() { _dumper->join(); }

          void wait_for_data_processing_finish();

          void process_cached_data();

          bool skip_reversible_block(uint32_t block);

          void cleanup_sequence()
          {
            ilog("Flushing rest of data, wait a moment...");

            process_cached_data();
            join_data_processors();

            ilog("Done, cleanup complete");
          }
        };

void sql_serializer_plugin_impl::wait_for_data_processing_finish()
{
  _dumper->wait_for_data_processing_finish();
}

void sql_serializer_plugin_impl::connect_signals()
{
  _on_pre_apply_operation_con = chain_db.add_pre_apply_operation_handler([&](const operation_notification& note) { on_pre_apply_operation(note); }, main_plugin);
  _on_pre_apply_block_con = chain_db.add_pre_apply_block_handler([&](const block_notification& note) { on_pre_apply_block(note); }, main_plugin);
  _on_post_apply_block_con = chain_db.add_post_apply_block_handler([&](const block_notification& note) { on_post_apply_block(note); }, main_plugin);
  _on_finished_reindex = chain_db.add_post_reindex_handler([&](const reindex_notification& note) { on_post_reindex(note); }, main_plugin);
  _on_starting_reindex = chain_db.add_pre_reindex_handler([&](const reindex_notification& note) { on_pre_reindex(note); }, main_plugin);
}

void sql_serializer_plugin_impl::disconnect_signals()
{
  if(_on_pre_apply_block_con.connected())
    chain::util::disconnect_signal(_on_pre_apply_block_con);

  if(_on_post_apply_block_con.connected())
    chain::util::disconnect_signal(_on_post_apply_block_con);
  if(_on_pre_apply_operation_con.connected())
    chain::util::disconnect_signal(_on_pre_apply_operation_con);
  if(_on_starting_reindex.connected())
    chain::util::disconnect_signal(_on_starting_reindex);
  if(_on_finished_reindex.connected())
    chain::util::disconnect_signal(_on_finished_reindex);

}

void sql_serializer_plugin_impl::on_pre_apply_block(const block_notification& note)
{
  ilog("Entering a resync data init for block: ${b}...", ("b", note.block_num));

  /// Let's init our database before applying first block (resync case)...
  init_database(note.block_num == 1, note.block_num);

  /// And disconnect to avoid subsequent inits
  if(_on_pre_apply_block_con.connected())
    chain::util::disconnect_signal(_on_pre_apply_block_con);
  ilog("Leaving a resync data init...");
}

void sql_serializer_plugin_impl::on_pre_apply_operation(const operation_notification& note)
{
  if(chain_db.is_producing())
  {
    dlog("Skipping operation processing coming from incoming transaction - waiting for already produced incoming block...");
    return;
  }

  if(skip_reversible_block(note.block))
    return;

  const bool is_virtual = hive::protocol::is_virtual_operation(note.op);

  cached_containter_t& cdtf = currently_caching_data; // alias

  ++op_sequence_id;

  cdtf->operations.emplace_back(
    op_sequence_id,
    note.block,
    note.trx_in_block,
    is_virtual && note.trx_in_block < 0 ? block_vops++ : note.op_in_trx,
    note.op
  );

}

void sql_serializer_plugin_impl::on_post_apply_block(const block_notification& note)
{
  FC_ASSERT(chain_db.is_producing() == false);

  if(skip_reversible_block(note.block_num))
    return;

  handle_transactions(note.block.transactions, note.block_num);

  currently_caching_data->total_size += note.block_id.data_size() + sizeof(note.block_num);
  currently_caching_data->blocks.emplace_back(
    note.block_id,
    note.block_num,
    note.block.timestamp,
    note.prev_block_id);
  block_vops = 0;
  _last_block_num = note.block_num;

  if(note.block_num % _dumper->blocks_per_flush() == 0)
  {
    process_cached_data();
  }

  if(note.block_num % 100'000 == 0)
  {
    log_statistics();
  }
}

void sql_serializer_plugin_impl::handle_transactions(const vector<hive::protocol::signed_transaction>& transactions, const int64_t block_num)
{
  uint trx_in_block = 0;

  for(auto& trx : transactions)
  {
    auto hash = trx.id();
    size_t sig_size = trx.signatures.size();

    currently_caching_data->total_size += sizeof(hash) + sizeof(block_num) + sizeof(trx_in_block) +
      sizeof(trx.ref_block_num) + sizeof(trx.ref_block_prefix) + sizeof(trx.expiration) + sizeof(trx.signatures[0]);

    currently_caching_data->transactions.emplace_back(
      hash,
      block_num,
      trx_in_block,
      trx.ref_block_num,
      trx.ref_block_prefix,
      trx.expiration,
      (sig_size == 0) ? fc::optional<signature_type>() : fc::optional<signature_type>(trx.signatures[0])
    );

    if(sig_size > 1)
    {
      auto itr = trx.signatures.begin() + 1;
      while(itr != trx.signatures.end())
      {
        currently_caching_data->transactions_multisig.emplace_back(
          hash,
          *itr
        );
        ++itr;
      }
    }

    trx_in_block++;
  }
}

void sql_serializer_plugin_impl::on_pre_reindex(const reindex_notification& note)
{
  ilog("Entering a reindex init...");
  /// Let's init our database before applying first block...
  init_database(note.force_replay, note.max_block_number);

  /// Disconnect pre-apply-block handler to avoid another initialization (for resync case).
  if(_on_pre_apply_block_con.connected())
    chain::util::disconnect_signal(_on_pre_apply_block_con);

  _dumper = std::make_unique< reindex_data_dumper >( db_url, psql_operations_threads_number, psql_transactions_threads_number );

  ilog("Leaving a reindex init...");
}

void sql_serializer_plugin_impl::on_post_reindex(const reindex_notification& note)
{
  ilog("finishing from post reindex");

  process_cached_data();
  wait_for_data_processing_finish();

  if(note.last_block_number >= note.max_block_number)
    switch_db_items(true/*mode*/);

  _dumper = std::make_unique< livesync_data_dumper >( db_url, main_plugin, chain_db );
}

void sql_serializer_plugin_impl::process_cached_data()
{  
  _dumper->trigger_data_flush( *currently_caching_data, _last_block_num );
}

bool sql_serializer_plugin_impl::skip_reversible_block(uint32_t block_no)
{
  if(block_no <= psql_block_number)
  {
    FC_ASSERT(block_no > chain_db.get_last_irreversible_block_num());

    if(last_skipped_block < block_no)
    {
      ilog("Skipping data provided by already processed reversible block: ${block_no}", ("block_no", block_no));
      last_skipped_block = block_no;
    }

    return true;
  }

  return false;
}
      } // namespace detail

      sql_serializer_plugin::sql_serializer_plugin() {}
      sql_serializer_plugin::~sql_serializer_plugin() {}


      void sql_serializer_plugin::set_program_options(appbase::options_description &cli, appbase::options_description &cfg)
      {
        cfg.add_options()("psql-url", boost::program_options::value<string>(), "postgres connection string")
                         ("psql-index-threshold", appbase::bpo::value<uint32_t>()->default_value( 1'000'000 ), "indexes/constraints will be recreated if `psql_block_number + psql_index_threshold >= head_block_number`")
                         ("psql-operations-threads-number", appbase::bpo::value<uint32_t>()->default_value( 5 ), "number of threads which dump operations to database during reindexing")
                         ("psql-transactions-threads-number", appbase::bpo::value<uint32_t>()->default_value( 2 ), "number of threads which dump transactions to database during reindexing");
      }

      void sql_serializer_plugin::plugin_initialize(const boost::program_options::variables_map &options)
      {
        ilog("Initializing sql serializer plugin");
        FC_ASSERT(options.count("psql-url"), "`psql-url` is required argument");

        auto& db = appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db();

        my = std::make_unique<detail::sql_serializer_plugin_impl>(options["psql-url"].as<fc::string>(), db, *this);

        // settings
        my->psql_index_threshold = options["psql-index-threshold"].as<uint32_t>();
        my->psql_operations_threads_number = options["psql-operations-threads-number"].as<uint32_t>();
        my->psql_transactions_threads_number = options["psql-transactions-threads-number"].as<uint32_t>();

        my->currently_caching_data = std::make_unique<cached_data_t>( default_reservation_size );

        // signals
        my->connect_signals();
      }

      void sql_serializer_plugin::plugin_startup()
      {
        ilog("sql::plugin_startup()");
      }

      void sql_serializer_plugin::plugin_shutdown()
      {
        ilog("Flushing left data...");
        my->join_data_processors();

        my->disconnect_signals();

        ilog("Done. Connection closed");
      }
      
    } // namespace sql_serializer
  }    // namespace plugins
} // namespace hive

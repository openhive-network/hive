#include <hive/plugins/sql_serializer/sql_serializer_plugin.hpp>

#include <hive/plugins/sql_serializer/data_processor.hpp>

#include <hive/plugins/sql_serializer/sql_serializer_objects.hpp>

#include <hive/plugins/sql_serializer/end_massive_sync_processor.hpp>

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
using chain::transaction_notification;
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

    typedef std::vector<PSQL::processing_objects::process_block_t> block_data_container_t;
    typedef std::vector<PSQL::processing_objects::process_transaction_t> transaction_data_container_t;
    typedef std::vector<PSQL::processing_objects::process_transaction_multisig_t> transaction_multisig_data_container_t;
    typedef std::vector<PSQL::processing_objects::process_operation_t> operation_data_container_t;


      using num_t = std::atomic<uint64_t>;
      using duration_t = fc::microseconds;
      using stat_time_t = std::atomic<duration_t>;

      inline void operator+=( stat_time_t& atom, const duration_t& dur )
      {
        atom.store( atom.load() + dur );
      }

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
      constexpr size_t max_tuples_count{ 1'000 };
      constexpr size_t max_data_length{ 16*1024*1024 }; 

      namespace detail
      {

      using data_processing_status = data_processor::data_processing_status;
      using data_chunk_ptr = data_processor::data_chunk_ptr;

      struct cached_data_t
      {

        block_data_container_t blocks;
        transaction_data_container_t transactions;
        transaction_multisig_data_container_t transactions_multisig;
        operation_data_container_t operations;

        size_t total_size;

        explicit cached_data_t(const size_t reservation_size) : total_size{ 0ul }
        {
          blocks.reserve(reservation_size);
          transactions.reserve(reservation_size);
          transactions_multisig.reserve(reservation_size);
          operations.reserve(reservation_size);
        }

        ~cached_data_t()
        {
          ilog(
          "blocks: ${b} trx: ${t} operations: ${o} total size: ${ts}...",
          ("b", blocks.size() )
          ("t", transactions.size() )
          ("o", operations.size() )
          ("ts", total_size )
          );
        }

      };
      using cached_containter_t = std::unique_ptr<cached_data_t>;

      /**
       * @brief Common implementation of data writer to be used for all SQL entities.
       * 
       * @tparam DataContainer temporary container providing a data chunk.
       * @tparam TupleConverter a functor to convert volatile representation (held in the DataContainer) into SQL representation
       *                        TupleConverter must match to function interface:
       *                        std::string(pqxx::work& tx, typename DataContainer::const_reference)
       * 
      */
      template <class DataContainer, class TupleConverter, const char* const TABLE_NAME, const char* const COLUMN_LIST>
      class container_data_writer
      {
      public:
        container_data_writer(std::string psqlUrl, std::string description, std::shared_ptr< trigger_synchronous_masive_sync_call > _api_trigger )
        {
          _processor = std::make_unique<queries_commit_data_processor>(psqlUrl, description, flush_replayed_data, _api_trigger);
        }

        void trigger(DataContainer&& data, bool wait_for_data_completion, uint32_t last_block_num)
        {
          if(data.empty() == false)
          {
            _processor->trigger(std::make_unique<chunk>(std::move(data)), last_block_num);
            if(wait_for_data_completion)
              _processor->complete_data_processing();
          } else {
            _processor->only_report_batch_finished( last_block_num );
          }

          FC_ASSERT(data.empty());
        }

        void complete_data_processing()
        {
          _processor->complete_data_processing();
        }

        void join()
        {
          _processor->join();
        }

      private:
        static data_processing_status flush_replayed_data(const data_chunk_ptr& dataPtr, transaction& tx)
        {
          const chunk* holder = static_cast<const chunk*>(dataPtr.get());
          data_processing_status processingStatus;

          TupleConverter conv;

          const DataContainer& data = holder->_data;

          FC_ASSERT(data.empty() == false);

          std::string query = "INSERT INTO ";
          query += TABLE_NAME;
          query += '(';
          query += COLUMN_LIST;
          query += ") VALUES\n";

          auto dataI = data.cbegin();
          query += '(' + conv(*dataI) + ")\n";

          for(++dataI; dataI != data.cend(); ++dataI)
          {
            query += ",(" + conv(*dataI) + ")\n";
          }

          query += ';';

          tx.exec(query);

          processingStatus.first += data.size();
          processingStatus.second = true;

          return processingStatus;

        }

        static data_processing_status flush_array_live_data(const data_chunk_ptr& dataPtr, transaction& tx)
        {
          const chunk* holder = static_cast<const chunk*>(dataPtr.get());
          data_processing_status processingStatus;

          TupleConverter conv;

          const DataContainer& data = holder->_data;

          FC_ASSERT(data.empty() == false);

          std::string query = "ARRAY[";

          auto dataI = data.cbegin();
          query += '(' + conv(*dataI) + ")\n";

          for(++dataI; dataI != data.cend(); ++dataI)
          {
            query += ",(" + conv(*dataI) + ")\n";
          }

          query += ']';

          processingStatus.first += data.size();
          processingStatus.second = true;

          return processingStatus;

        }

        static data_processing_status flush_scalar_live_data(const data_chunk_ptr& dataPtr, transaction& tx)
        {
          const chunk* holder = static_cast<const chunk*>(dataPtr.get());
          data_processing_status processingStatus;

          TupleConverter conv;

          const DataContainer& data = holder->_data;

          FC_ASSERT(data.empty() == false);

          std::string query = "";

          auto dataI = data.cbegin();
          query += '(' + conv(*dataI) + ")\n";

          for(++dataI; dataI != data.cend(); ++dataI)
          {
            query += ",(" + conv(*dataI) + ")\n";
          }

          processingStatus.first += data.size();
          processingStatus.second = true;

          return processingStatus;

        }


        private:
          class chunk : public data_processor::data_chunk
          {
          public:
            chunk( DataContainer&& data) : _data(std::move(data)) {}
            virtual ~chunk() {}

            virtual std::string generate_code(size_t* processedItem) const override { return std::string(); }

            DataContainer _data;
          };

      private:
        std::unique_ptr<queries_commit_data_processor> _processor;
        transaction_controller_ptr      _prev_tx_controller;
      };

      template <typename TableDescriptor>
      using table_data_writer = container_data_writer<typename TableDescriptor::container_t, typename TableDescriptor::data2sql_tuple,
        TableDescriptor::TABLE, TableDescriptor::COLS>;

          struct data2_sql_tuple_base
          {
          explicit data2_sql_tuple_base(){}

          protected:
              std::string escape(const std::string& source) const
              {
                return escape_sql(source);
              }

              std::string escape_raw(const fc::ripemd160& hash) const
              {
                return '\'' + fc::to_hex(hash.data(), hash.data_size()) + '\'';
              }

              std::string escape_raw(const fc::optional<signature_type>& sign) const
              {
                if( sign.valid() )
                  return '\'' + fc::to_hex(reinterpret_cast<const char*>( sign->begin() ), sign->size()) + '\'';
                else
                  return "NULL";
              }

          private:

              fc::string escape_sql(const std::string &text) const
              {
                if(text.empty()) return "E''";

                std::wstring utf32;
                utf32.reserve( text.size() );
                fc::decodeUtf8( text, &utf32 );

                std::string ret;
                ret.reserve( 6 * text.size() );

                ret = "E'";

                for (auto it = utf32.begin(); it != utf32.end(); it++)
                {

                  const wchar_t& c{*it};
                  const int code = static_cast<int>(c);

                  if( code == 0 ) ret += " ";
                  if(code > 0 && code <= 0x7F && std::isprint(code)) // if printable ASCII
                  {
                    switch(c)
                    {
                      case L'\r': ret += "\\015"; break;
                      case L'\n': ret += "\\012"; break;
                      case L'\v': ret += "\\013"; break;
                      case L'\f': ret += "\\014"; break;
                      case L'\\': ret += "\\134"; break;
                      case L'\'': ret += "\\047"; break;
                      case L'%':  ret += "\\045"; break;
                      case L'_':  ret += "\\137"; break;
                      case L':':  ret += "\\072"; break;
                      default:    ret += static_cast<char>(code); break;
                    }
                  }
                  else
                  {
                    fc::string u_code{}; 
                    u_code.reserve(8);

                    const int i_c = int(c);
                    const char * c_str = reinterpret_cast<const char*>(&i_c);
                    for( int _s = ( i_c > 0xff ) + ( i_c > 0xffff ) + ( i_c > 0xffffff ); _s >= 0; _s-- ) 
                      u_code += fc::to_hex( c_str + _s, 1 );

                    if(i_c > 0xffff)
                    {
                      ret += "\\U";
                      if(u_code.size() < 8) ret.insert( ret.end(), 8 - u_code.size(), '0' );
                    }
                    else 
                    {
                      ret += "\\u";
                      if(u_code.size() < 4) ret.insert( ret.end(), 4 - u_code.size(), '0' );
                    }
                    ret += u_code;
                  }
                }

                ret += '\'';
                return ret;
              }
          };

      struct hive_blocks
      {
        typedef block_data_container_t container_t;

        static const char TABLE[];
        static const char COLS[];

        struct data2sql_tuple : public data2_sql_tuple_base
        {
          using data2_sql_tuple_base::data2_sql_tuple_base;

          std::string operator()(typename container_t::const_reference data) const
          {
            return std::to_string(data.block_number) + "," + escape_raw(data.hash) + "," +
              escape_raw(data.prev_hash) + ", '" + data.created_at.to_iso_string() + '\'';
          }
        };
      };

      const char hive_blocks::TABLE[] = "hive.blocks";
      const char hive_blocks::COLS[] = "num, hash, prev, created_at";

      struct hive_transactions
      {
        typedef transaction_data_container_t container_t;

        static const char TABLE[];
        static const char COLS[];

        struct data2sql_tuple : public data2_sql_tuple_base
        {
          using data2_sql_tuple_base::data2_sql_tuple_base;

          std::string operator()(typename container_t::const_reference data) const
          {
            return std::to_string(data.block_number) + "," + escape_raw(data.hash) + "," + std::to_string(data.trx_in_block) + "," +
                   std::to_string(data.ref_block_num) + "," + std::to_string(data.ref_block_prefix) + ",'" + data.expiration.to_iso_string() + "'," + escape_raw(data.signature);
          }
        };
      };

      const char hive_transactions::TABLE[] = "hive.transactions";
      const char hive_transactions::COLS[] = "block_num, trx_hash, trx_in_block, ref_block_num, ref_block_prefix, expiration, signature";

      struct hive_transactions_multisig
      {
        typedef transaction_multisig_data_container_t container_t;

        static const char TABLE[];
        static const char COLS[];

        struct data2sql_tuple : public data2_sql_tuple_base
        {
          using data2_sql_tuple_base::data2_sql_tuple_base;

          std::string operator()(typename container_t::const_reference data) const
          {
            return escape_raw(data.hash) + "," + escape_raw(data.signature);
          }
        };
      };

      const char hive_transactions_multisig::TABLE[] = "hive.transactions_multisig";
      const char hive_transactions_multisig::COLS[] = "trx_hash, signature";

      struct hive_operations
      {
        typedef operation_data_container_t container_t;

        static const char TABLE[];
        static const char COLS[];

        struct data2sql_tuple : public data2_sql_tuple_base
        {
          using data2_sql_tuple_base::data2_sql_tuple_base;

          std::string operator()(typename container_t::const_reference data) const
          {
            // deserialization
            fc::variant opVariant;
            fc::to_variant(data.op, opVariant);
            fc::string deserialized_op = fc::json::to_string(opVariant);

            return std::to_string(data.operation_id) + ',' + std::to_string(data.block_number) + ',' +
              std::to_string(data.trx_in_block) + ',' + std::to_string(data.op_in_trx) + ',' +
              std::to_string(data.op.which()) + ',' + escape(deserialized_op);
          }
        };
      };

      const char hive_operations::TABLE[] = "hive.operations";
      const char hive_operations::COLS[] = "id, block_num, trx_in_block, op_pos, op_type_id, body";

      using block_data_container_t_writer = table_data_writer<hive_blocks>;
      using transaction_data_container_t_writer = table_data_writer<hive_transactions>;
      using transaction_multisig_data_container_t_writer = table_data_writer<hive_transactions_multisig>;
      using operation_data_container_t_writer = table_data_writer<hive_operations>;



        struct transaction_repr_t
        {
          std::unique_ptr<pqxx::connection> _connection;
          std::unique_ptr<pqxx::work> _transaction;

          transaction_repr_t() = default;
          transaction_repr_t(pqxx::connection* _conn, pqxx::work* _trx) : _connection{_conn}, _transaction{_trx} {}

          auto get_escaping_charachter_methode() const
          {
            return [this](const char *val) -> fc::string { return std::move( this->_connection->esc(val) ); };
          }

          auto get_raw_escaping_charachter_methode() const
          {
            return [this](const char *val, const size_t s) -> fc::string { 
              pqxx::binarystring __tmp(val, s); 
              return std::move( this->_transaction->esc_raw( __tmp.str() ) ); 
            };
          }
        };
        using transaction_t = std::unique_ptr<transaction_repr_t>;

        struct postgress_connection_holder
        {
          explicit postgress_connection_holder(const fc::string &url)
              : connection_string{url} {}

          transaction_t start_transaction() const
          {
            pqxx::connection* _conn = new pqxx::connection{ this->connection_string };
            pqxx::work *_trx = new pqxx::work{*_conn};
            _trx->exec("SET CONSTRAINTS ALL DEFERRED;");

            return transaction_t{ new transaction_repr_t{ _conn, _trx } };
          }

          bool exec_transaction(transaction_t &trx, const fc::string &sql) const
          {
            if (sql == fc::string())
              return true;
            return sql_safe_execution([&trx, &sql]() { trx->_transaction->exec(sql); }, sql.c_str());
          }

          bool commit_transaction(transaction_t &trx) const
          {
            return sql_safe_execution([&]() { trx->_transaction->commit(); }, "commit");
          }

          void abort_transaction(transaction_t &trx) const
          {
            trx->_transaction->abort();
          }

          bool exec_single_in_transaction(const fc::string &sql, pqxx::result *result = nullptr) const
          {
            if (sql == fc::string())
              return true;

            return sql_safe_execution([&]() {
              pqxx::connection conn{ this->connection_string };
              pqxx::work trx{conn};
              if (result)
                *result = trx.exec(sql);
              else
                trx.exec(sql);
              trx.commit();
            }, sql.c_str());
          }

          template<typename T>
          bool get_single_value(const fc::string& query, T& _return) const
          {
            pqxx::result res;
            if(!exec_single_in_transaction( query, &res ) && res.empty() ) return false;
            _return = res.at(0).at(0).as<T>();
            return true;
          }

          template<typename T>
          T get_single_value(const fc::string& query) const
          {
            T _return;
            FC_ASSERT( get_single_value( query, _return ) );
            return _return;
          }

          uint get_max_transaction_count() const
          {
            // get maximum amount of connections defined by postgres and set half of it as max; minimum is 1
            return std::max( 1u, get_single_value<uint>("SELECT setting::int / 2 FROM pg_settings WHERE  name = 'max_connections'") );
          }

        private:
          fc::string connection_string;

          bool sql_safe_execution(const std::function<void()> &f, const char* msg = nullptr) const
          {
            try
            {
              f();
              return true;
            }
            catch (const pqxx::pqxx_exception &sql_e)
            {
              elog("Exception from postgress: ${e}", ("e", sql_e.base().what()));
            }
            catch (const std::exception &e)
            {
              elog("Exception: ${e}", ("e", e.what()));
            }
            catch (...)
            {
              elog("Unknown exception occured");
            }
            if(msg) std::cerr << "Error message: " << msg << std::endl;
            return false;
          }
        };

        class sql_serializer_plugin_impl final
        {
        public:
          sql_serializer_plugin_impl(const std::string &url, hive::chain::database& _chain_db, const sql_serializer_plugin& _main_plugin) 
            : connection{url},
              db_url{url},
              chain_db{_chain_db},
              main_plugin{_main_plugin}
          {

            init_data_processors();
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

          std::unique_ptr<block_data_container_t_writer> _block_writer;
          std::unique_ptr<transaction_data_container_t_writer> _transaction_writer;
          std::unique_ptr<transaction_multisig_data_container_t_writer> _transaction_multisig_writer;
          std::unique_ptr<operation_data_container_t_writer> _operation_writer;
          std::unique_ptr<end_massive_sync_processor> _end_massive_sync_processor;

          postgress_connection_holder connection;
          std::string db_url;
          hive::chain::database& chain_db;
          const sql_serializer_plugin& main_plugin;

          uint32_t _last_block_num = 0;

          uint32_t last_skipped_block = 0;
          uint32_t psql_block_number = 0;
          uint32_t psql_index_threshold = 0;
          uint32_t head_block_number = 0;

          uint32_t blocks_per_commit = 1;
          int64_t block_vops = 0;
          int64_t op_sequence_id = 0; 

          cached_containter_t currently_caching_data;
          stats_group current_stats;

          bool massive_sync = false;

          void log_statistics()
          {
            std::cout << current_stats;
            current_stats.reset();
          }

          void switch_db_items( bool mode, const std::string& function_name, const std::string& objects_name ) const
          {
            ilog("${mode} ${objects_name}...", ("objects_name", objects_name )("mode", ( mode ? "Creating" : "Dropping" ) ) );

            std::string query = std::string("SELECT ") + function_name + "();";
            std::string description = "Query processor: `" + query + "'";
            queries_commit_data_processor processor(db_url, description, [query, &objects_name, mode](const data_chunk_ptr&, transaction& tx) -> data_processing_status
                          {
                            ilog("Attempting to execute query: `${query}`...", ("query", query ) );
                            const auto start_time = fc::time_point::now();
                            tx.exec( query );
                            ilog(
                              "${mode} of ${mod_type} done in ${time} ms",
                              ("mode", (mode ? "Creating" : "Saving and dropping")) ("mod_type", objects_name) ("time", (fc::time_point::now() - start_time).count() / 1000.0 )
                            );
                            return data_processing_status();
                          } , nullptr);

            processor.trigger(data_processor::data_chunk_ptr(), 0);
            processor.join();

            ilog("The ${objects_name} have been ${mode}...", ("objects_name", objects_name )("mode", ( mode ? "created" : "dropped" ) ) );
          }

          void switch_db_items( bool create ) const
          {
            if( psql_block_number == 0 || ( psql_block_number + psql_index_threshold <= head_block_number ) )
            {
              ilog("Switching indexes and constraints is allowed: psql block number: `${pbn}` psql index threshold: `${pit}` head block number: `${hbn}` ...",
              ("pbn", psql_block_number)("pit", psql_index_threshold)("hbn", head_block_number));

              if(create)
              {
                switch_db_items(create, "hive.enable_indexes_of_irreversible", "enable indexes" );
              }
              else
              {
                switch_db_items(create, "hive.disable_indexes_of_irreversible", "disable indexes" );
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

            FC_ASSERT( is_database_correct(), "Database is in invalid state" );

            load_initial_db_data();

            if(freshDb)
            {
              connection.exec_single_in_transaction(PSQL::get_all_type_definitions());
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
                    data_processing_status processingStatus;
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
                data_processing_status processingStatus;
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
                data_processing_status processingStatus;
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

          void trigger_data_flush(block_data_container_t& data);
          void trigger_data_flush(transaction_data_container_t& data);
          void trigger_data_flush(transaction_multisig_data_container_t& data);
          void trigger_data_flush(operation_data_container_t& data);

          void init_data_processors();
          void join_data_processors()
          {
            _block_writer->join();
            _transaction_writer->join();
            _transaction_multisig_writer->join();
            _operation_writer->join();
            _end_massive_sync_processor->join();
          }

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

          void commit_massive_sync( int block_num );
        };

void sql_serializer_plugin_impl::init_data_processors()
{
  _end_massive_sync_processor = std::make_unique< end_massive_sync_processor >( db_url );
  constexpr auto NUMBER_OF_PROCESSORS_THREADS = 4;
  auto execute_end_massive_sync_callback = [this](trigger_synchronous_masive_sync_call::BLOCK_NUM _block_num ){
    _end_massive_sync_processor->trigger_block_number( _block_num );
  };
  auto api_trigger = std::make_shared< trigger_synchronous_masive_sync_call >( NUMBER_OF_PROCESSORS_THREADS, execute_end_massive_sync_callback );
  _block_writer = std::make_unique<block_data_container_t_writer>(db_url, "Block data writer", api_trigger);
  _transaction_writer = std::make_unique<transaction_data_container_t_writer>(db_url, "Transaction data writer", api_trigger);
  _transaction_multisig_writer = std::make_unique<transaction_multisig_data_container_t_writer>(db_url, "Transaction multisig data writer", api_trigger);
  _operation_writer = std::make_unique<operation_data_container_t_writer>(db_url, "Operation data writer", api_trigger);
}


void sql_serializer_plugin_impl::wait_for_data_processing_finish()
{
  _block_writer->complete_data_processing();
  _transaction_writer->complete_data_processing();
  _transaction_multisig_writer->complete_data_processing();
  _operation_writer->complete_data_processing();
  _end_massive_sync_processor->complete_data_processing();
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
    ilog("Skipping operation processing coming from incoming transaction - waiting for already produced incoming block...");
    return;
  }

  if(skip_reversible_block(note.block))
    return;

  const bool is_virtual = hive::protocol::is_virtual_operation(note.op);

  detail::cached_containter_t& cdtf = currently_caching_data; // alias

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

  if(note.block_num % blocks_per_commit == 0)
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

  blocks_per_commit = 1'000;
  massive_sync = true;

  ilog("Leaving a reindex init...");
}

void sql_serializer_plugin_impl::on_post_reindex(const reindex_notification& note)
{
  ilog("finishing from post reindex");

  process_cached_data();
  wait_for_data_processing_finish();

  if(note.last_block_number >= note.max_block_number)
    switch_db_items(true/*mode*/);

  blocks_per_commit = 1;
  massive_sync = false;
}


void sql_serializer_plugin_impl::trigger_data_flush(block_data_container_t& data)
{
  _block_writer->trigger(std::move(data), massive_sync == false, _last_block_num);
}

void sql_serializer_plugin_impl::trigger_data_flush(transaction_data_container_t& data)
{
  _transaction_writer->trigger(std::move(data), massive_sync == false, _last_block_num);
}

void sql_serializer_plugin_impl::trigger_data_flush(transaction_multisig_data_container_t& data)
{
  _transaction_multisig_writer->trigger(std::move(data), massive_sync == false, _last_block_num);
}

void sql_serializer_plugin_impl::trigger_data_flush(operation_data_container_t& data)
{
  _operation_writer->trigger(std::move(data), massive_sync == false, _last_block_num);
}

void sql_serializer_plugin_impl::process_cached_data()
{  
  auto* data = currently_caching_data.get();

  trigger_data_flush(data->blocks);
  trigger_data_flush(data->operations);

  trigger_data_flush(data->transactions);
  trigger_data_flush(data->transactions_multisig);
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

void sql_serializer_plugin_impl::commit_massive_sync(int block_num) {

}

      } // namespace detail

      sql_serializer_plugin::sql_serializer_plugin() {}
      sql_serializer_plugin::~sql_serializer_plugin() {}


      void sql_serializer_plugin::set_program_options(appbase::options_description &cli, appbase::options_description &cfg)
      {
        cfg.add_options()("psql-url", boost::program_options::value<string>(), "postgres connection string")
                         ("psql-index-threshold", appbase::bpo::value<uint32_t>()->default_value( 1'000'000 ), "indexes/constraints will be recreated if `psql_block_number + psql_index_threshold >= head_block_number`");
      }

      void sql_serializer_plugin::plugin_initialize(const boost::program_options::variables_map &options)
      {
        ilog("Initializing sql serializer plugin");
        FC_ASSERT(options.count("psql-url"), "`psql-url` is required argument");

        auto& db = appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db();

        my = std::make_unique<detail::sql_serializer_plugin_impl>(options["psql-url"].as<fc::string>(), db, *this);

        // settings
        my->psql_index_threshold = options["psql-index-threshold"].as<uint32_t>();

        my->currently_caching_data = std::make_unique<detail::cached_data_t>( default_reservation_size );

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

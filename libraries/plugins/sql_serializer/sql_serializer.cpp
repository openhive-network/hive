#include <hive/plugins/sql_serializer/sql_serializer_plugin.hpp>

#include <hive/chain/util/impacted.hpp>
#include <hive/protocol/config.hpp>
#include <hive/utilities/plugin_utilities.hpp>

#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>
#include <boost/format.hpp>

// PSQL
#include <pqxx/pqxx>

namespace hive
{
  namespace plugins
  {
    namespace sql_serializer
    {
      using namespace hive::protocol;

      using chain::database;
      using chain::operation_notification;
      using chain::operation_object;

      namespace detail
      {
        class sql_serializer_plugin_impl
        {q
          using namespace hive::plugins::sql_serializer::PSQL;

        public:
          sql_serializer_plugin_impl(const std::string &url) : _db(appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db()),
                                                               connection_url{url}
          {
          }

          virtual ~sql_serializer_plugin_impl()
          {
            if (_log_file.is_open())
            {
              _log_file.close();
            }
          }

          database &_db;
          std::string connection_url;
          bool recreate_db = false;
          bool set_index = true;

          std::shared_ptr<std::thread> worker;
          PSQL::queue<PSQL::post_process_command> &qu

              std::map<uint64_t, std::string>
                  names_to_flush;
          const std::map<TABLE, p_str_que_str> flushes{
              {table_flush_values_cell_t{TABLE::BLOCKS, p_str_que_str{"hive_blocks"}},
               table_flush_values_cell_t{TABLE::TRANSACTIONS, p_str_que_str{"hive_transactions"}},
               table_flush_values_cell_t{TABLE::ACCOUNTS, p_str_que_str{"hive_accounts"}},
               table_flush_values_cell_t{TABLE::OPERATIONS, p_str_que_str{"hive_operations"}},
               table_flush_values_cell_t{TABLE::OPERATION_NAMES, p_str_que_str{"hive_operation_names"}},
               table_flush_values_cell_t{TABLE::DETAILS_ASSET, p_str_que_str{"hive_operation_details_asset"}},
               table_flush_values_cell_t{TABLE::DETAILS_PERMLINK, p_str_que_str{"hive_operation_details_permlink"}},
               table_flush_values_cell_t{TABLE::DETAILS_ID, p_str_que_str{"hive_operation_details_id"}},
               table_flush_values_cell_t{TABLE::ASSET_DICT, p_str_que_str{"hive_asset_dictionary"}},
               table_flush_values_cell_t{TABLE::PERMLINK_DICT, p_str_que_str{"hive_permlink_dictionary"}}}};
        };

        std::string asset_num_to_string(uint32_t asset_num)
        {
          switch (asset_num)
          {
#ifdef IS_TEST_NET
          case HIVE_ASSET_NUM_HIVE:
            return "TESTS";
          case HIVE_ASSET_NUM_HBD:
            return "TBD";
#else
          case HIVE_ASSET_NUM_HIVE:
            return "HIVE";
          case HIVE_ASSET_NUM_HBD:
            return "HBD";
#endif
          case HIVE_ASSET_NUM_VESTS:
            return "VESTS";
          default:
            return "UNKN";
          }
        }

      } // namespace detail

      sql_serializer_plugin::sql_serializer_plugin() {}
      sql_serializer_plugin::~sql_serializer_plugin() {}

      void sql_serializer_plugin::set_program_options(options_description &cli, options_description &cfg)
      {
        cfg.add_options()("psql-url", boost::program_options::value<string>(), "postgres connection string")
        ("psql-recreate-db", "recreates whole database")
        ("psql-reindex-on-exit", "creates indexes and PK on exit" );
      }

      void sql_serializer_plugin::plugin_initialize(const boost::program_options::variables_map &options)
      {
        ilog("Initializing sql serializer plugin");
        FC_ASSERT(options.count("psql-url"), "`psql-url` is required argument")
        my = std::make_unique<detail::sql_serializer_plugin_impl>(options["psql-url"].as<std::string>());

        // settings
        my->recreate_db = options.count("psql-recreate-db") > 0;
        my->set_index = options.count("psql-reindex-on-exit") > 0;
      }

      void sql_serializer_plugin::plugin_startup()
      {
        my->worker = std::make_shared<std::thread>([&]() {
          connection conn{my->connection_url.c_str()};

          // flush all data for given table
          auto flusher = [&](const TABLE k) {
            FC_ASSERT(conn.is_open());
            auto &it = my->flushes.find(k)->second;
            if (it.to_dump->empty())
              return;
            std::stringstream ss;
            std::string str;

            it.to_dump->pull(str);
            ss << "INSERT INTO " << it.table_name << " VALUES " << str;
            while (!it.to_dump->empty())
            {
              it.to_dump->pull(str);
              ss << "," << str;
            }
            if (k == TABLE::PERMLINK_DICT || k == TABLE::ASSET_DICT)
            {
              ss << " ON CONFLICT DO NOTHING";
            }
            nontransaction w{conn};
            w.exec(ss.str());
          };

          // worker
          post_process_command ret;
          my->qu.wait_pull(ret);

          // work loop
          while (ret.op.valid())
          {
            // process
            api_operation_object _api_obj{*ret.op};
            _api_obj.operation_id = ret.op_id;
            const sql_command_t vec = _api_obj.op.visit(
                PSQL::sql_serializer_visitor{
                    _api_obj.block,
                    _api_obj.trx_in_block,
                    _api_obj.op_in_trx,
                    _api_obj.op.which(),        /* operation type */
                    _api_obj.op.virtual_op > 0, /* is_virtual */
                    ret.op->impacted            /* impacted */
                });

            // flush
            for (const auto &v : vec)
            {
              flushes.at(v.first).to_dump->push(v.second);
              if (flushes.at(v.first).to_dump->size() == buff_size)
                flusher(v.first);
            }

            // gather next element
            my->qu.wait_pull(ret);
          }

          // flush what hasn't been already flushed
          for (auto &kv : flushes)
            flusher(kv.first);

        });
      }

      void sql_serializer_plugin::plugin_shutdown() 
      {
        ilog("Shutting down sql serializer plugin in progress");
        my->qu.wait_push( PSQL::post_process_command{} );
        my->worker.join();
        ilog("Shutting down sql serializer plugin, done");
      }

    } // namespace sql_serializer
  }   // namespace plugins
} // namespace hive
} // namespace hive
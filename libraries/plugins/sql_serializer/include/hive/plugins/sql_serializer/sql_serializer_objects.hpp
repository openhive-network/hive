#pragma once

// STL
#include <thread>
#include <queue>
#include <future>
#include <sstream>
#include <string>
#include <functional>
#include <shared_mutex>

// Boost
#include <boost/thread/sync_queue.hpp>
#include <boost/type_index.hpp>

// Internal
#include <hive/chain/database.hpp>
#include <hive/chain/util/impacted.hpp>
#include <hive/chain/util/extractors.hpp>
#include <hive/chain/account_object.hpp>
// #include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>

namespace hive
{
  namespace plugins
  {
    namespace sql_serializer
    {
      using operation_types_t = std::map<int64_t, std::pair<fc::string, bool>>;
      using asset_container_t = std::map<hive::protocol::asset_symbol_type, uint16_t>;

      namespace PSQL
      {
        template <typename T>
        using queue = boost::concurrent::sync_queue<T>;
        using hive::protocol::asset;
        using hive::protocol::operation;
        using hive::protocol::signed_block;
        using hive::protocol::signed_transaction;

        using namespace PSQL;

        enum TABLE
        {
          // basics
          BLOCKS = 0,
          TRANSACTIONS = 1,
          ACCOUNTS = 2,

          // operations
          OPERATION_NAMES = 3,
          OPERATIONS = 4,
          VIRTUAL_OPERATIONS = 5,

          // let's safe some place
          PERMLINK_DICT = 11,

          END = 99
        };

        // using counter_t = std::atomic<uint32_t>;
        using counter_t = uint32_t;
        using counter_container_t = std::map<TABLE, counter_t>;
        using counter_container_member_t = std::pair<TABLE, PSQL::counter_t>;

        using que_str = queue<std::string>;
        using mtx_t = std::mutex;

        struct is_virtual_visitor
        {
          typedef bool result_type;

          template<typename op_t>
          bool operator()(const op_t& op) const
          {
            return op.is_virtual();
          }
        };
        
        struct p_str_que_str
        {
          std::string table_name;
          std::shared_ptr<que_str> to_dump{new que_str{}};

          explicit p_str_que_str(const std::string &s) : table_name{s} {}
        };
        using table_flush_values_cell_t = std::pair<const TABLE, p_str_que_str>;
        const size_t buff_size = 1000;

        const std::vector<const char *> init_database_commands{{}};
        const std::vector<const char *> end_database_commands{{}};

        struct processing_object
        {
          fc::string hash;
          std::shared_ptr<operation> op;
          int64_t block_number;
          fc::optional<int64_t> trx_in_block;
          fc::optional<int64_t> op_in_trx;
          int64_t virtual_op;

          bool valid() const { return hash != fc::string() || op.get() != nullptr; }
        };

        struct command
        {
          TABLE first = END;
          std::string second = "";

          command() = default;
          command(const TABLE tbl, const std::string &str) : first{tbl}, second{str} {}
        };

        using sql_command_t = std::list<command>;
        using strstrm = std::stringstream;
        using namespace hive::protocol;
        using escape_function_t = std::function<fc::string(const char *)>;

        inline std::string generate(std::function<void(strstrm &)> fun)
        {
          std::stringstream ss;
          fun(ss);
          return ss.str();
        }

        struct sql_serializer_visitor
        {
          typedef fc::string result_type;

          const hive::chain::database &db;
          sql_command_t &result;
          int64_t block_number;
          int64_t trx_in_block;

          counter_t op_id;
          int64_t op_in_trx;
          bool is_virtual;

          escape_function_t escape = [](const char *val) { return fc::string{val}; };

          template <typename operation_t>
          result_type operator()(const operation_t &op) const
          {
            get_basic_info(op);
            process(op);
            return op_type_name<operation_t>();
          }

          void get_basic_info(const operation &op) const
          {
            result.emplace_back(
                // TABLE::OPERATIONS,
                (is_virtual ? TABLE::VIRTUAL_OPERATIONS : TABLE::OPERATIONS),
                generate([&](strstrm &ss) { ss << "( "
                                               << op_id << " , "
                                               << block_number << " , "
                                               << ( is_virtual && trx_in_block < 0 ? "NULL" : std::to_string(trx_in_block) ) << ", "
                                               << ( is_virtual && trx_in_block < 0 ? "NULL" : std::to_string(op_in_trx) ) << " , "
                                              //  << op_in_trx << " /* op_in_trx */, "
                                               << op.which() << " , "
                                               << format_participants(op)
                                               << get_formatted_permlinks(op, is_virtual)
                                               << ")"; }));
          }

          void create_account(const account_name_type &acc) const
          {
            const auto &accnt = db.get_account(acc);
            result.emplace_back(
                TABLE::ACCOUNTS,
                generate([&](strstrm &s) { s << "( " << accnt.get_id() << ", '" << fc::string{acc} << "' )"; }));
          }

        private:
          template <typename T>
          fc::string op_type_name() const
          {
            return boost::typeindex::type_id<T>().pretty_name();
          }

          fc::string format_participants(const operation &op) const
          {
            boost::container::flat_set<account_name_type> impacted;
            hive::app::operation_get_impacted_accounts(op, impacted);
            impacted.erase(account_name_type());
            return generate([&](strstrm &ss) {
              ss << "'{";
              for (auto it = impacted.begin(); it != impacted.end(); it++)
                ss << (it == impacted.begin() ? " " : ", ") << db.get_account(*it).get_id();
              ss << " }'::INT[]";
            });
          }

          fc::string get_formatted_permlinks(const operation &op, const bool is_virtual) const
          {
            if (is_virtual)
              return "";
            std::vector<fc::string> result;
            hive::app::operation_get_permlinks(op, result);
            return generate([&](strstrm &ss) {
              ss << ", ARRAY[]::INT[]";

              for (auto it = result.begin(); it != result.end(); it++)
                ss
                    << " || get_inserted_permlink_id( E'"
                    << escape((*it).c_str())
                    << "' )";
            });
          }

          template <typename operation_t>
          void process(const operation_t &op) const {}
        };

        struct sql_serializer_processor
        {
          const processing_object &input;
          operation_types_t &op_types;
          counter_container_t &counters;
          escape_function_t escape;

          const hive::chain::database &db = appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db();

          void process_block(sql_command_t &sql_commands) const
          {
            sql_commands.emplace_back(TABLE::BLOCKS, generate([&](strstrm &ss) {
                                        ss << "( " << input.block_number << " , '" << input.hash << "' )";
                                      }));
          }

          void process_transaction(sql_command_t &sql_commands) const
          {
              sql_commands.emplace_back(TABLE::TRANSACTIONS, generate([&](strstrm &ss) {
                                          ss << "( " << input.block_number << " , " << *input.trx_in_block << " , '" << input.hash << "' )";
                                        }));
          }

          void process_operation(sql_command_t &sql_commands) const
          {
            const bool is_virtual = input.op->visit( is_virtual_visitor{} );
            counters[(is_virtual ? TABLE::VIRTUAL_OPERATIONS : TABLE::OPERATIONS)]++;
            op_types[input.op->which()] = std::make_pair(input.op->visit(sql_serializer_visitor{
                                                              db,
                                                              sql_commands,
                                                              input.block_number,
                                                              *input.trx_in_block,
                                                              counters[(is_virtual ? TABLE::VIRTUAL_OPERATIONS : TABLE::OPERATIONS)],
                                                              *input.op_in_trx,
                                                              is_virtual,
                                                              escape
                                                          }),
                                                          is_virtual);
          }

          sql_command_t operator()() const
          {
            sql_command_t result;

            if (!input.op_in_trx.valid())
            {
              if (!input.trx_in_block.valid())
                process_block(result);
              else
                process_transaction(result);
            }
            else process_operation(result);

            return result;
          }
        };

      } // namespace PSQL
    }   // namespace sql_serializer
  }     // namespace plugins
} // namespace hive

template <>
inline void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const hive::protocol::account_create_operation &op) const
{
  create_account(op.new_account_name);
}

template <>
inline void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const hive::protocol::account_create_with_delegation_operation &op) const
{
  create_account(op.new_account_name);
}
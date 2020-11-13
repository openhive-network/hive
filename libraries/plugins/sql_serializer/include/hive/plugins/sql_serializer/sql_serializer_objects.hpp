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
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>

namespace hive
{
  namespace plugins
  {
    namespace sql_serializer
    {
      namespace PSQL
      {
        template <typename T>
        using queue = boost::concurrent::sync_queue<T>;
        using hive::plugins::account_history_rocksdb::account_history_rocksdb_plugin;
        using hive::plugins::account_history_rocksdb::rocksdb_operation_object;
        using hive::plugins::account_history::api_operation_object;
        using hive::protocol::signed_block;

        using namespace PSQL;

        enum TABLE
        {
          // basics
          BLOCKS = 0,
          TRANSACTIONS = 1,
          ACCOUNTS = 2,

          // operation stuff
          OPERATIONS = 3,
          OPERATION_NAMES = 4,

          // operation details
          DETAILS_ASSET = 5,
          DETAILS_PERMLINK = 6,
          DETAILS_ID = 7,

          // let's safe some place
          ASSET_DICT = 8,
          PERMLINK_DICT = 9,

          END
        };

        using que_str = queue<std::string>;
        using mtx_t = std::mutex;
        struct p_str_que_str
        {
          std::string table_name;
          std::shared_ptr<que_str> to_dump{new que_str{}};

          explicit p_str_que_str(const std::string &s) : table_name{s} {}
        };
        using table_flush_values_cell_t = std::pair<const TABLE, p_str_que_str>;
        const size_t buff_size = 1000;

        const std::vector<const char *> init_database_commands{
            {R"(DROP TABLE IF EXISTS hive_blocks CASCADE)",
             R"(DROP TABLE IF EXISTS hive_transactions CASCADE)",
             R"(DROP TABLE IF EXISTS hive_operation_names CASCADE)",
             R"(DROP TABLE IF EXISTS hive_operations CASCADE)",
             R"(DROP TABLE IF EXISTS hive_asset_dictionary CASCADE)",
             R"(DROP TABLE IF EXISTS hive_operation_details_asset CASCADE)",
             R"(DROP TABLE IF EXISTS hive_permlink_dictionary CASCADE)",
             R"(DROP TABLE IF EXISTS hive_operation_details_permlink CASCADE)",
             R"(DROP TABLE IF EXISTS hive_operation_details_id CASCADE)",
             R"(CREATE TABLE IF NOT EXISTS hive_blocks ( num INTEGER NOT NULL, hash CHARACTER (40) NOT NULL, created_at timestamp WITHOUT time zone NOT NULL ))",
             R"(CREATE TABLE IF NOT EXISTS hive_transactions ( block_num INTEGER NOT NULL, trx_pos INTEGER NOT NULL, trx_hash character (40) NOT NULL ))",
             R"(CREATE TABLE IF NOT EXISTS hive_operation_names ( id INTEGER NOT NULL, "name" TEXT NOT NULL ))",
             R"(CREATE TABLE IF NOT EXISTS hive_operations ( block_num INTEGER NOT NULL, trx_pos INTEGER NOT NULL, op_pos INTEGER NOT NULL, op_name_id INTEGER NOT NULL, is_virtual boolean NOT NULL DEFAULT FALSE, participants char(16)[] ))",
             R"(CREATE TABLE IF NOT EXISTS hive_asset_dictionary ( "id" BIGINT PRIMARY KEY NOT NULL, "nai" CHARACTER(11) NOT NULL, "precision" smallint NOT NULL ))",
             R"(CREATE TABLE IF NOT EXISTS hive_operation_details_asset ( block_num INTEGER NOT NULL, trx_pos INTEGER NOT NULL, op_pos INTEGER NOT NULL, "amount" BIGINT NOT NULL, "symbol" BIGINT NOT NULL ))",
             R"(CREATE TABLE IF NOT EXISTS hive_permlink_dictionary ( "permlink_hash" BYTEA PRIMARY KEY NOT NULL, "permlink" TEXT NOT NULL ))",
             R"(CREATE TABLE IF NOT EXISTS hive_operation_details_permlink ( block_num INTEGER NOT NULL, trx_pos INTEGER NOT NULL, op_pos INTEGER NOT NULL, "permlink_hash" BYTEA NOT NULL))",
             R"(CREATE TABLE IF NOT EXISTS hive_operation_details_id ( block_num INTEGER NOT NULL, trx_pos INTEGER NOT NULL, op_pos INTEGER NOT NULL, "id" BIGINT NOT NULL );)"}};

        const std::vector<const char *> end_database_commands{
            {R"(ALTER TABLE hive_blocks ADD PRIMARY KEY (num);)",
             R"(ALTER TABLE hive_transactions ADD PRIMARY KEY (block_num, trx_pos);)",
             R"(ALTER TABLE hive_transactions ADD CONSTRAINT hive_transactions_fk_1 FOREIGN KEY (block_num) REFERENCES hive_blocks (num);)",
             R"(ALTER TABLE hive_operation_names ADD PRIMARY KEY (id);)",
             R"(ALTER TABLE hive_operations ADD CONSTRAINT hive_operations_fk_2 FOREIGN KEY (op_name_id) REFERENCES hive_operation_names (id);)",
             R"(CREATE INDEX IF NOT EXISTS hive_operations_operation_types_index on "hive_operations" (op_name_id);)",
             R"(CREATE INDEX IF NOT EXISTS hive_operations_participants_index on "hive_operations" USING GIN ("participants");)",
             R"(ALTER TABLE hive_operation_details_asset ADD PRIMARY KEY (block_num, trx_pos, op_pos);)",
             R"(ALTER TABLE hive_operation_details_asset ADD CONSTRAINT hive_operation_details_asset_fk_1 FOREIGN KEY (symbol) REFERENCES hive_asset_dictionary (id);)",
             R"(ALTER TABLE hive_operation_details_permlink ADD CONSTRAINT hive_operation_details_permlink_fk_2 FOREIGN KEY (permlink_hash) REFERENCES hive_permlink_dictionary (permlink_hash);)"}};

        struct command
        {
          TABLE first = END;
          std::string second = "";

          command() = default;
          command(const TABLE tbl, const std::string &str) : first{tbl}, second{str} {}
        };

        struct post_process_command
        {
          using optional_op_t = fc::optional<rocksdb_operation_object>;
          post_process_command() : op{} {}
          post_process_command(const rocksdb_operation_object &obj, const uint64_t _op_id) : op{obj}, op_id{_op_id} {}
          
					optional_op_t op;
          uint64_t op_id = ~(0ull);
        };

        using sql_command_t = std::vector<command>;
        using ssman = std::stringstream;
        using namespace hive::protocol;

        struct sql_serializer_visitor
        {
          typedef sql_command_t result_type;

          int64_t block_num = 0;
          int64_t trx_pos = 0;
          int64_t op_position = 0;
          int64_t tag = 0;
          bool is_virtual = false;
          const std::vector<account_name_type> &impacted;

          template <typename operation_t>
          sql_command_t operator()(const operation_t &op) const
          {
            add_type_id<operation_t>();
            auto ret = get_basic_info(op);
            process(op, ret);
            return ret;
          }

          template <typename operation_t>
          sql_command_t get_basic_info(const operation_t &op) const
          {
            return sql_command_t{
                command{
                    TABLE::OPERATIONS,
                    generate([&](ssman &ss) { ss << "( " << block_num << ", " << trx_pos << ", " << op_position << ", " << tag << ", " << (is_virtual ? "TRUE" : "FALSE") << ", " << generate([&](ssman &ss2) {
                                                if (impacted.size() == 0)
                                                {
                                                  ss2 << "'{}'";
                                                  return;
                                                }
                                                ss2 << "'{\"" << std::string{impacted.front()} << '"';
                                                for (size_t i = 1; i < impacted.size(); i++)
                                                  ss2 << ", \"" << std::string{impacted[i]} << '"';
                                                ss2 << "}'";
                                              })
                                                 << ")"; })}};
          }

          void get_asset_details(const asset &a, sql_command_t &ret) const
          {
            ret.emplace_back(
                TABLE::DETAILS_ASSET,
                generate([&](ssman &s) { s << "( " << block_num << ", " << trx_pos << ", " << op_position << ", " << a.amount.value << ", " << a.symbol.asset_num << " )"; }));
            ret.emplace_back(
                TABLE::ASSET_DICT,
                generate([&](ssman &s) { s << "( " << a.symbol.asset_num << ", '" << a.symbol.to_nai_string() << "', " << static_cast<int>(a.symbol.decimals()) << " )"; }));
          }

          template <typename num_t>
          void get_id_details(const num_t num, sql_command_t &ret) const
          {
            static_assert(std::is_arithmetic<num_t>());
            ret.emplace_back(
                TABLE::DETAILS_ID,
                generate([&](ssman &s) { s << "( " << block_num << ", " << trx_pos << ", " << op_position << ", " << num << " )"; }));
          }

          void get_permlink_details(const fc::string &str, sql_command_t &ret) const
          {
            const fc::string hex = fc::sha256::hash(str.c_str(), str.size()).str();
            ret.emplace_back(
                TABLE::DETAILS_PERMLINK,
                generate([&](ssman &s) { s << "( " << block_num << ", " << trx_pos << ", " << op_position << ", decode('" << hex << "', 'hex') )"; }));
            ret.emplace_back(
                TABLE::PERMLINK_DICT,
                generate([&](ssman &s) { s << "( decode('" << hex << "', 'hex') , '" << str << "' )"; }));
          }

        private:
          template <typename T>
          void add_type_id() const
          {
            names_to_flush[tag] = boost::typeindex::type_id<T>().pretty_name();
          }

          std::string generate(std::function<void(ssman &)> fun) const
          {
            std::stringstream ss;
            fun(ss);
            return ss.str();
          }

          template <typename operation_t>
          void process(const operation_t &op, sql_command_t &ret) const {}
        };

      } // namespace PSQL
    }   // namespace sql_serializer
  }     // namespace plugins
}       // namespace hive

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const transfer_to_vesting_operation &op, sql_command_t &ret) const
{
  get_asset_details(op.amount, ret);
}

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const transfer_operation &op, sql_command_t &ret) const
{
  get_asset_details(op.amount, ret);
}

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const account_witness_vote_operation &op, sql_command_t &ret) const
{
  get_id_details(op.approve, ret);
}

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const comment_operation &op, sql_command_t &ret) const
{
  get_permlink_details(op.permlink, ret);
}

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const vote_operation &op, sql_command_t &ret) const
{
  get_permlink_details(op.permlink, ret);
}

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const withdraw_vesting_operation &op, sql_command_t &ret) const
{
  get_asset_details(op.vesting_shares, ret);
}

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const custom_operation &op, sql_command_t &ret) const
{
  get_id_details(op.id, ret);
}

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const limit_order_create_operation &op, sql_command_t &ret) const
{
  get_id_details(op.orderid, ret);
}

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const limit_order_cancel_operation &op, sql_command_t &ret) const
{
  get_id_details(op.orderid, ret);
}

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const delete_comment_operation &op, sql_command_t &ret) const
{
  get_permlink_details(op.permlink, ret);
}

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const set_withdraw_vesting_route_operation &op, sql_command_t &ret) const
{
  get_id_details(op.percent, ret);
}

template <>
void hive::plugins::sql_serializer::PSQL::sql_serializer_visitor::process(const convert_operation &op, sql_command_t &ret) const
{
  get_id_details(op.requestid, ret);
}

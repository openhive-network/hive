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
      // [which] = ( op_name, is_virtual )
      using operation_types_container_t = std::map<int64_t, std::pair<fc::string, bool>>;
      namespace PSQL
      {
        template <typename T>
        using queue = boost::concurrent::sync_queue<T>;
        using hive::protocol::asset;
        using hive::protocol::operation;
        using hive::protocol::signed_block;
        using hive::protocol::signed_transaction;

        using namespace hive::protocol;

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
          PERMLINK_DICT = 6,

          END
        };

        // using counter_t = std::atomic<uint32_t>;
        using counter_t = uint32_t;
        using counter_container_t = std::map<TABLE, counter_t>;
        using counter_container_member_t = std::pair<TABLE, PSQL::counter_t>;
        using sql_insertion_buffer = queue<fc::string>;

        struct is_virtual_visitor
        {
          typedef bool result_type;

          template <typename op_t>
          bool operator()(const op_t &op) const
          {
            return op.is_virtual();
          }
        };
        struct name_gathering_visitor
        { 
          using result_type = fc::string; 

          template<typename op_t>
          result_type operator()(const op_t&) const
          {
            return boost::typeindex::type_id<op_t>().pretty_name();
          }
        };

        struct table_flush_buffer
        {
          std::string table_name;
          std::shared_ptr<sql_insertion_buffer> to_dump{new sql_insertion_buffer{}};

          explicit table_flush_buffer(const fc::string &s) : table_name{s} {}
        };
        using table_flush_values_cell_t = std::pair<const TABLE, table_flush_buffer>;

        namespace processing_objects
        {
          struct process_block_t
          {
            fc::string hash;
            int64_t block_number;

            process_block_t() = default;
            process_block_t(const fc::string& _hash, const int64_t _block_number) :
              hash{_hash}, block_number{_block_number} {}
          };

          struct process_transaction_t : public process_block_t
          {
            int64_t trx_in_block;

            process_transaction_t() = default;
            process_transaction_t(const fc::string& _hash, const int64_t _block_number, const int64_t _trx_in_block) :
              process_block_t{_hash, _block_number}, trx_in_block{_trx_in_block} {}
          };

          struct process_operation_t
          {
            uint64_t id;
            int64_t block_number;
            int64_t trx_in_block;
            int64_t op_in_trx;
            fc::optional<operation> op;

            process_operation_t() = default;
            process_operation_t( const uint64_t _id, const int64_t _block_number, const int64_t _trx_in_block, const int64_t _op_in_trx, const operation& _op ) :
              id{ _id }, block_number{ _block_number}, trx_in_block{ _trx_in_block}, op_in_trx{_op_in_trx}, op{_op} {}
          };

          struct end_processing_t{};
          struct is_processable_visitor
          {
            using result_type = bool;

            template<typename T>
            result_type operator()(const T&) const { return true; }
          };

        }; // namespace processing_objects

        using processing_object_t = fc::static_variant<
          processing_objects::process_operation_t,
          processing_objects::process_transaction_t,
          processing_objects::process_block_t,
          processing_objects::end_processing_t
        >;

        struct command
        {
          TABLE destination_table = END;
          fc::string insertion_tuple = "";

          command() = default;
          command(const TABLE tbl, const std::string &str) : destination_table{tbl}, insertion_tuple{str} {}
        };

        using sql_command_t = std::list<command>;
        using strstrm = std::stringstream;
        using escape_function_t = std::function<fc::string(const char *)>;

        inline std::string generate(std::function<void(strstrm &)> fun)
        {
          std::stringstream ss;
          fun(ss);
          return ss.str();
        }

        struct sql_dump_visitor
        {
          typedef void result_type;

          const hive::chain::database &db;
          sql_command_t &result;
          operation_types_container_t &op_types;
          escape_function_t escape;

          result_type operator()(const processing_objects::end_processing_t &bop) const {}

          result_type operator()(const processing_objects::process_block_t &bop) const
          {
            result.emplace_back(TABLE::BLOCKS, generate([&](strstrm &ss) {
                                  ss << "( " << bop.block_number << " , '" << bop.hash << "' )";
                                }));
          }

          result_type operator()(const processing_objects::process_transaction_t &top) const
          {
            result.emplace_back(TABLE::TRANSACTIONS, generate([&](strstrm &ss) {
                                  ss << "( " << top.block_number << " , " << top.trx_in_block << " , '" << top.hash << "' )";
                                }));
          }

          result_type operator()(const processing_objects::process_operation_t &pop) const
          {

            op_types[pop.op->which()] = std::make_pair<fc::string, bool>(
              pop.op->visit(name_gathering_visitor{}),
              pop.op->visit(is_virtual_visitor{})
            );
            get_operation_basic_info(pop);
            additional_details(*pop.op);
          }

          void get_operation_basic_info(const processing_objects::process_operation_t &pop) const
          {
            const bool is_virtual = pop.op->visit(is_virtual_visitor{});
            result.emplace_back(
                (is_virtual ? TABLE::VIRTUAL_OPERATIONS : TABLE::OPERATIONS),
                generate([&](strstrm &ss) {  ss << "( "
                                                << pop.id << " , "
                                                << pop.block_number << " , "
                                                << (is_virtual && pop.trx_in_block < 0 ? "NULL" : std::to_string(pop.trx_in_block)) << ", "
                                                << (is_virtual && pop.trx_in_block < 0 ? "NULL" : std::to_string(pop.op_in_trx)) << " , "
                                                << pop.op->which() << " , "
                                                << format_participants(*pop.op)
                                                << get_formatted_permlinks(*pop.op, is_virtual)
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
          void additional_details(const operation_t &op) const {}
        };
      } // namespace PSQL
    }   // namespace sql_serializer
  }     // namespace plugins
} // namespace hive

template <>
inline void hive::plugins::sql_serializer::PSQL::sql_dump_visitor::additional_details(const hive::protocol::account_create_operation &op) const
{
  create_account(op.new_account_name);
}

template <>
inline void hive::plugins::sql_serializer::PSQL::sql_dump_visitor::additional_details(const hive::protocol::account_create_with_delegation_operation &op) const
{
  create_account(op.new_account_name);
}

template<>
inline bool hive::plugins::sql_serializer::PSQL::processing_objects::is_processable_visitor::operator()(const hive::plugins::sql_serializer::PSQL::processing_objects::end_processing_t&) const 
{
  return false;
}
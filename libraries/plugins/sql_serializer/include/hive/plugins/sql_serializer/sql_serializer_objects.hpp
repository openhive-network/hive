#pragma once

// STL
#include <sstream>
#include <string>
#include <functional>

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
        using escape_function_t = std::function<fc::string(const char *)>;

        using hive::protocol::operation;

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
            int64_t block_number;
            int64_t trx_in_block;
            int64_t op_in_trx;
            fc::optional<operation> op;

            process_operation_t() = default;
            process_operation_t( const int64_t _block_number, const int64_t _trx_in_block, const int64_t _op_in_trx, const operation& _op ) :
              block_number{ _block_number}, trx_in_block{ _trx_in_block}, op_in_trx{_op_in_trx}, op{_op} {}
          };

          struct commit_data_t{};

          struct end_processing_t{};

          struct is_processable_visitor
          {
            using result_type = bool;

            template<typename T>
            result_type operator()(const T&) const { return true; }
          };

          struct commit_command_visitor
          {
            using result_type = bool;

            template<typename T>
            result_type operator()(const T&) const { return false; }
          };

        }; // namespace processing_objects

        using processing_object_t = fc::static_variant<
          processing_objects::process_operation_t,
          processing_objects::process_transaction_t,
          processing_objects::process_block_t,
          processing_objects::commit_data_t,
          processing_objects::end_processing_t
        >;

        using strstrm = std::stringstream;
        inline std::string generate(std::function<void(strstrm &)> fun)
        {
          strstrm ss;
          fun(ss);
          return ss.str();
        }

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

        struct body_gathering_visitor
        {
          using result_type = fc::string;

          template<typename operation_t>
          result_type operator()(const operation_t&) const { return fc::string{}; }

          template<typename op_t>
          result_type get_body(const op_t& op) const { return fc::json::to_string(op); }
        };

        struct sql_dump_visitor
        {
          typedef fc::string result_type;

          const hive::chain::database &db;
          operation_types_container_t &op_types;
          escape_function_t escape;

          template<typename T>
          result_type operator()(const T &) const { return fc::string{}; }

        private:

          result_type process_operation(const processing_objects::process_operation_t &pop) const
          {
            return generate([&](strstrm &ss) {  ss << "INSERT INTO hive_operations(block_num, trx_in_block, op_pos, op_type_id, participants, permlink_ids, body) VALUES ( "
                                                << pop.block_number << " , "
                                                << pop.trx_in_block << ", "
                                                << pop.op_in_trx << " , "
                                                << get_operation_type_id(*pop.op, false) << " , "
                                                << format_participants(*pop.op) << ", "
                                                << get_formatted_permlinks(*pop.op) << " , "
                                                << get_body(*pop.op)
                                                << ")"; });
          }

          result_type process_virtual_operation(const processing_objects::process_operation_t &pop) const
          {
            return generate([&](strstrm &ss) {  ss << "INSERT INTO hive_virtual_operations(block_num, trx_in_block, op_pos, op_type_id, participants, body) VALUES ( "
                                                << pop.block_number << " , "
                                                << pop.trx_in_block << ", "
                                                << pop.op_in_trx << " , "
                                                << get_operation_type_id(*pop.op, true) << " , "
                                                << format_participants(*pop.op) << " , "
                                                << get_body(*pop.op)
                                                << ")"; });
          }

          fc::string format_participants(const operation &op) const
          {
            using hive::protocol::account_name_type;
            boost::container::flat_set<account_name_type> impacted;
            hive::app::operation_get_impacted_accounts(op, impacted);
            impacted.erase(account_name_type());
            return generate([&](strstrm &ss) {
              ss << "'{}'::INT[]";
              for (const auto& it : impacted)
                ss << " || get_inserted_account_id( '" << fc::string(it) << "' )";
            });
          }

          fc::string get_formatted_permlinks(const operation &op) const
          {
            std::vector<fc::string> result;
            hive::app::operation_get_permlinks(op, result);
            return generate([&](strstrm &ss) {
              ss << "'{}'::INT[]";
              for (const auto& it : result)
                ss << " || get_inserted_permlink_id( E'" << escape(fc::string(it).c_str()) << "' )";
            });
          }

          fc::string get_body(const operation& op) const
          {
            const fc::string body = op.visit( body_gathering_visitor{} );
            if(body == fc::string{}) return "NULL";
            return generate([&](strstrm &ss) {
              ss << "E'" << escape(body.c_str()) << "'";
            });
          }

          fc::string get_operation_type_id(const operation& op, const bool is_virtual) const
          {
            const size_t size = op_types.size();
            const auto op_name = op.visit(name_gathering_visitor{});
            op_types[op.which()] = std::pair<fc::string, bool>( op_name.c_str(), is_virtual );

            if(size < op_types.size())
              return generate([&](strstrm &ss){ ss << "insert_operation_type_id( " << op.which() << ", '" << op_name << "', " << (is_virtual ? "TRUE" : "FALSE") << ") "; });
            else 
              return std::to_string(op.which());
          }
        };

      } // namespace PSQL
    }   // namespace sql_serializer
  }     // namespace plugins
} // namespace hive

template<>
inline bool hive::plugins::sql_serializer::PSQL::processing_objects::is_processable_visitor::operator()(const hive::plugins::sql_serializer::PSQL::processing_objects::end_processing_t&) const 
{
  return false;
}

template<>
inline bool hive::plugins::sql_serializer::PSQL::processing_objects::commit_command_visitor::operator()(const hive::plugins::sql_serializer::PSQL::processing_objects::commit_data_t&) const 
{
  return true;
}

template<>
inline fc::string hive::plugins::sql_serializer::PSQL::sql_dump_visitor::operator()
(const hive::plugins::sql_serializer::PSQL::processing_objects::process_block_t &bop) const
{
  return generate([&](strstrm &ss) {
                        ss << "INSERT INTO hive_blocks VALUES ( " << bop.block_number << " , '" << bop.hash << "' )";
                      });
}

template<>
inline fc::string hive::plugins::sql_serializer::PSQL::sql_dump_visitor::operator()
(const hive::plugins::sql_serializer::PSQL::processing_objects::process_transaction_t &top) const
{
  return generate([&](strstrm &ss) {
                        ss << "INSERT INTO hive_transactions VALUES ( " << top.block_number << " , " << top.trx_in_block << " , '" << top.hash << "' )";
                      });
}

template<>
inline fc::string hive::plugins::sql_serializer::PSQL::sql_dump_visitor::operator()
(const hive::plugins::sql_serializer::PSQL::processing_objects::process_operation_t &pop) const
{
  const bool is_virtual = pop.op->visit(is_virtual_visitor{});
  if(is_virtual) return process_virtual_operation(pop);
  else return process_operation(pop);
}

#pragma once

// STL
#include <sstream>
#include <string>
#include <atomic>
#include <functional>

// Boost
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/copy_if.hpp>
#include <boost/mpl/not.hpp>
#include <boost/type.hpp>
#include <boost/type_index.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/thread/sync_queue.hpp>
#include <boost/spirit/include/karma.hpp>

// Internal
#include <hive/chain/util/extractors.hpp>
#include <hive/chain/account_object.hpp>
#include "type_extractor_processor.hpp"

namespace hive
{
  namespace plugins
  {
    namespace sql_serializer
    {
      namespace PSQL
      {

        using operation_types_container_t = std::map<int64_t, std::pair<fc::string, bool>>;

        struct typename_gatherer
        {
          operation_types_container_t &names;

          template <typename T, typename SV>
          void operator()(boost::type<boost::tuple<T, SV>> t) const
          {
            names[names.size()] = std::pair<fc::string, bool>(boost::typeindex::type_id<T>().pretty_name(), T{}.is_virtual());
          }
        };

        template <typename T>
        using queue = boost::concurrent::sync_queue<T>;
        using escape_function_t = std::function<fc::string(const char *)>;
        using escape_raw_function_t = std::function<fc::string(const char *, const size_t)>;

        using hive::protocol::operation;
        using hive::protocol::signature_type;

        namespace processing_objects
        {
          struct process_base_t
          {
            using hash_t = fc::ripemd160;

            hash_t hash;

            process_base_t() = default;
            process_base_t(const hash_t &_hash) : hash{_hash} {}
          };

          struct process_base_ex_t : public process_base_t
          {
            int block_number;

            process_base_ex_t() = default;
            process_base_ex_t(const hash_t &_hash, const int _block_number) : process_base_t(_hash), block_number{_block_number} {}
          };

          struct process_block_t : public process_base_ex_t
          {
            fc::time_point_sec created_at;
            hash_t prev_hash;

            process_block_t() = default;
            process_block_t(const hash_t &_hash, const int _block_number, const fc::time_point_sec _tp, const hash_t &_prev)
              : process_base_ex_t{_hash, _block_number}, created_at{_tp}, prev_hash{_prev} {}
          };

          struct process_transaction_t : public process_base_ex_t
          {
            using process_base_t::hash_t;

            int16_t trx_in_block;
            uint16_t ref_block_num;
            uint32_t ref_block_prefix;
            fc::optional<signature_type> signature;

            process_transaction_t() = default;
            process_transaction_t(const hash_t& _hash, const int _block_number, const int16_t _trx_in_block,
                                  const uint16_t _ref_block_num, const uint32_t _ref_block_prefix, const fc::optional<signature_type>& _signature)
            : process_base_ex_t{_hash, _block_number}, trx_in_block{_trx_in_block},
              ref_block_num{_ref_block_num}, ref_block_prefix{_ref_block_prefix}, signature{_signature}
            {}
          };

          struct process_transaction_multisig_t : public process_base_t
          {
            using process_base_t::hash_t;

            signature_type signature;

            process_transaction_multisig_t(const hash_t& _hash, const signature_type& _signature)
            : process_base_t{_hash}, signature{_signature}
            {}
          };

          struct process_operation_t
          {
            int64_t operation_id;
            int32_t block_number;
            int16_t trx_in_block;
            int16_t op_in_trx;
            operation op;

            process_operation_t(int64_t _operation_id, int32_t _block_number, const int16_t _trx_in_block, const int16_t _op_in_trx,
              const operation &_op) : operation_id{_operation_id }, block_number{_block_number}, trx_in_block{_trx_in_block},
              op_in_trx{_op_in_trx}, op{_op} {}
          };

          /// Holds account information to be put into database
          struct account_data_t
          {
            account_data_t(int _id, std::string _n) : id{_id}, name {_n} {}

            int32_t id;
            std::string name;
          };

          /// Holds permlink information to be put into database
          struct permlink_data_t
          {
            permlink_data_t(int _id, std::string _p) : id{_id}, permlink{_p} {}

            int32_t id;
            std::string permlink;
          };

          /// Holds association between account and its operations.
          struct account_operation_data_t
          {
            int64_t operation_id;
            int32_t account_id;
            int32_t operation_seq_no;

            account_operation_data_t(int64_t _operation_id, int32_t _account_id, int32_t _operation_seq_no) : operation_id{ _operation_id },
              account_id{ _account_id }, operation_seq_no{ _operation_seq_no } {}
          };

        }; // namespace processing_objects

        inline fc::string generate(std::function<void(fc::string &)> fun)
        {
          fc::string ss;
          fun(ss);
          return std::move(ss);
        }


        struct name_gathering_visitor
        {
          using result_type = fc::string;

          template <typename op_t>
          result_type operator()(const op_t &) const
          {
            return boost::typeindex::type_id<op_t>().pretty_name();
          }
        };

        constexpr const char *SQL_bool(const bool val) { return (val ? "TRUE" : "FALSE"); }

        inline fc::string get_all_type_definitions()
        {
          namespace te = type_extractor;
          typedef te::sv_processor<hive::protocol::operation> processor;

          operation_types_container_t result;
          typename_gatherer p{result};
          boost::mpl::for_each<processor::transformed_type_list, boost::type<boost::mpl::_>>(p);

          if (result.empty())
            return fc::string{};
          else
          {
            return generate([&](fc::string &ss) {
              ss.append("INSERT INTO hive_operation_types VALUES ");
              for (auto it = result.begin(); it != result.end(); it++)
              {
                if (it != result.begin())
                  ss.append(",");
                ss.append("( ");
                ss.append(std::to_string(it->first));
                ss.append(" , '");
                ss.append(it->second.first);
                ss.append("', ");
                ss.append(SQL_bool(it->second.second));
                ss.append(" )");
              }
              ss.append(" ON CONFLICT DO NOTHING");
            });
          }
        }
        using cache_contatiner_t = std::set<fc::string>;

      } // namespace PSQL
    }    // namespace sql_serializer
  }      // namespace plugins
} // namespace hive
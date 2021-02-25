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
#include <boost/multi_index/composite_key.hpp>

// Internal
#include <hive/chain/util/extractors.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/chain/hive_object_types.hpp>
#include "type_extractor_processor.hpp"

namespace hive
{
  namespace plugins
  {
    namespace sql_serializer
    {
      using chainbase::shared_string;
      using chainbase::t_vector;
      using buffer_type = t_vector< char >;

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
            uint32_t block_number = 0;

            process_base_ex_t() = default;
            process_base_ex_t(const hash_t &_hash, const uint32_t _block_number) : process_base_t(_hash), block_number{_block_number} {}
          };

          struct process_block_t : public process_base_ex_t
          {
            fc::time_point_sec created_at;
            hash_t prev_hash;

            process_block_t() = default;
            process_block_t(const hash_t &_hash, const uint32_t _block_number, const fc::time_point_sec _tp, const hash_t &_prev)
              : process_base_ex_t{_hash, _block_number}, created_at{_tp}, prev_hash{_prev} {}
          };

          struct process_transaction_t : public process_base_ex_t
          {
            using process_base_t::hash_t;

            uint16_t trx_in_block = 0;
            uint16_t ref_block_num = 0;
            uint32_t ref_block_prefix = 0;
            fc::time_point_sec expiration;
            fc::optional<signature_type> signature;

            process_transaction_t() = default;
            process_transaction_t(const hash_t& _hash, const uint32_t _block_number, const uint16_t _trx_in_block,
                                  const uint16_t _ref_block_num, const uint32_t _ref_block_prefix, const fc::time_point_sec& _expiration, const fc::optional<signature_type>& _signature)
            : process_base_ex_t{_hash, _block_number}, trx_in_block{_trx_in_block},
              ref_block_num{_ref_block_num}, ref_block_prefix{_ref_block_prefix}, expiration{_expiration}, signature{_signature}
            {}
          };

          struct process_transaction_multisig_t : public process_base_t
          {
            using process_base_t::hash_t;

            signature_type signature;

            process_transaction_multisig_t() = default;
            process_transaction_multisig_t(const hash_t& _hash, const signature_type& _signature)
            : process_base_t{_hash}, signature{_signature}
            {}
          };

          struct process_transaction_multisig_block_t : public process_transaction_multisig_t
          {
            uint32_t block_number = 0;

            process_transaction_multisig_block_t() = default;
            process_transaction_multisig_block_t(const hash_t& _hash, const signature_type& _signature, const uint32_t _block_number)
            : process_transaction_multisig_t{_hash, _signature}, block_number{_block_number}
            {}
          };

          template<typename operation_type>
          struct _process_operation_t
          {
            int64_t operation_id = 0;
            uint32_t block_number = 0;
            int16_t trx_in_block = 0;
            int16_t op_in_trx = 0;
            operation_type op;

            template<typename Allocator>
            _process_operation_t(const Allocator& a): op{a} {}

            _process_operation_t() = default;
            _process_operation_t(int64_t _operation_id, uint32_t _block_number, const int16_t _trx_in_block, const int16_t _op_in_trx,
              const operation_type &_op) : operation_id{_operation_id }, block_number{_block_number}, trx_in_block{_trx_in_block},
              op_in_trx{_op_in_trx}, op{_op} {}
          };
          using process_operation_t = _process_operation_t<operation>;
          using volatile_process_operation_t = _process_operation_t<buffer_type>;

          /// Holds account information to be put into database
          template<typename string_type>
          struct _account_data_t
          {
            template<typename Allocator>
            _account_data_t(const Allocator& a)
                            : name{a} {}

            _account_data_t(int _id, string_type _n) : id{_id}, name {_n} {}

            int32_t id = 0;
            string_type name;
          };
          using account_data_t = _account_data_t<std::string>;
          using volatile_account_data_t = _account_data_t<shared_string>;

          /// Holds permlink information to be put into database
          template<typename string_type>
          struct _permlink_data_t
          {
            template<typename Allocator>
            _permlink_data_t(const Allocator& a)
                            : permlink{a} {}

            _permlink_data_t(int _id, string_type _p)
                            : id{_id}, permlink{_p} {}

            int32_t id = 0;
            string_type permlink;
          };
          using permlink_data_t = _permlink_data_t<std::string>;
          using volatile_permlink_data_t = _permlink_data_t<shared_string>;

          struct account_data_block_t : public volatile_account_data_t
          {
            template<typename Allocator>
            account_data_block_t(const Allocator& a)
                          : volatile_account_data_t{a} {}

            account_data_block_t(int _id, shared_string _n, const uint32_t _block_number)
                                : volatile_account_data_t{_id, _n}, block_number{_block_number} {}

            uint32_t block_number;
          };

          struct permlink_data_block_t : public volatile_permlink_data_t
          {
            template<typename Allocator>
            permlink_data_block_t( const Allocator& a )
                          : volatile_permlink_data_t{a} {}

            permlink_data_block_t(int _id, shared_string _p, const uint32_t _block_number)
                          : volatile_permlink_data_t{_id, _p}, block_number{_block_number} {}

            uint32_t block_number;
          };

          /// Holds association between account and its operations.
          struct account_operation_data_t
          {
            int64_t operation_id = 0;
            int32_t account_id = 0;
            int32_t operation_seq_no = 0;

            account_operation_data_t() = default;
            account_operation_data_t(int64_t _operation_id, int32_t _account_id, int32_t _operation_seq_no) : operation_id{ _operation_id },
              account_id{ _account_id }, operation_seq_no{ _operation_seq_no } {}
          };

          struct account_operation_data_block_t: public account_operation_data_t
          {
            uint32_t block_number = 0;

            account_operation_data_block_t() = default;
            account_operation_data_block_t(int64_t _operation_id, int32_t _account_id, int32_t _operation_seq_no, const uint32_t _block_number)
                                      : account_operation_data_t{ _operation_id, _account_id, _operation_seq_no }, block_number{_block_number} {}
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

        using namespace hive::chain;

        constexpr uint16_t base_hive_type(uint16_t n = 8)
        {
          const uint16_t HIVE_ACCOUNT_HISTORY_SQL_SPACE_ID = 15;
          return HIVE_ACCOUNT_HISTORY_SQL_SPACE_ID << n;
        }

        enum account_history_sql_object_types
        {
          volatile_account_data_object_type         = base_hive_type(),
          volatile_permlink_data_object_type        = base_hive_type() + 1,
          volatile_block_object_type                = base_hive_type() + 2,
          volatile_transaction_object_type          = base_hive_type() + 3,
          volatile_transaction_multisig_object_type = base_hive_type() + 4,
          volatile_operation_object_type            = base_hive_type() + 5,
          volatile_account_operation_object_type    = base_hive_type() + 6
        };

        struct by_block;

        class volatile_account_data_object : public object< volatile_account_data_object_type, volatile_account_data_object >
        {
          CHAINBASE_OBJECT( volatile_account_data_object );

          public:
            CHAINBASE_DEFAULT_CONSTRUCTOR(volatile_account_data_object, (account_data))

            processing_objects::account_data_block_t account_data;

            uint32_t get_block_number() const
            {
              return account_data.block_number;
            }

            CHAINBASE_UNPACK_CONSTRUCTOR(volatile_account_data_object, (account_data));
        };
        typedef multi_index_container<
            volatile_account_data_object,
            indexed_by<
              ordered_unique< tag< by_id >,
                const_mem_fun< volatile_account_data_object, volatile_account_data_object::id_type, &volatile_account_data_object::get_id > >,
              ordered_unique< tag< by_block >,
                composite_key< volatile_account_data_object,
                  const_mem_fun< volatile_account_data_object, uint32_t, &volatile_account_data_object::get_block_number >,
                  const_mem_fun< volatile_account_data_object, volatile_account_data_object::id_type, &volatile_account_data_object::get_id >
                >
              >
            >,
            allocator< volatile_account_data_object >
          > volatile_account_data_index;

        class volatile_permlink_data_object : public object< volatile_permlink_data_object_type, volatile_permlink_data_object >
        {
          CHAINBASE_OBJECT( volatile_permlink_data_object );

          public:
            CHAINBASE_DEFAULT_CONSTRUCTOR(volatile_permlink_data_object, (permlink_data))

            processing_objects::permlink_data_block_t permlink_data;

            uint32_t get_block_number() const
            {
              return permlink_data.block_number;
            }

            CHAINBASE_UNPACK_CONSTRUCTOR(volatile_permlink_data_object, (permlink_data));
        };
        typedef multi_index_container<
            volatile_permlink_data_object,
            indexed_by<
              ordered_unique< tag< by_id >,
                const_mem_fun< volatile_permlink_data_object, volatile_permlink_data_object::id_type, &volatile_permlink_data_object::get_id > >,
              ordered_unique< tag< by_block >,
                composite_key< volatile_permlink_data_object,
                  const_mem_fun< volatile_permlink_data_object, uint32_t, &volatile_permlink_data_object::get_block_number >,
                  const_mem_fun< volatile_permlink_data_object, volatile_permlink_data_object::id_type, &volatile_permlink_data_object::get_id >
                >
              >
            >,
            allocator< volatile_permlink_data_object >
          > volatile_permlink_data_index;

        class volatile_block_object : public object< volatile_block_object_type, volatile_block_object >
        {
          CHAINBASE_OBJECT( volatile_block_object );

          public:
            CHAINBASE_DEFAULT_CONSTRUCTOR(volatile_block_object)

            processing_objects::process_block_t block;

            uint32_t get_block_number() const
            {
              return block.block_number;
            }

            CHAINBASE_UNPACK_CONSTRUCTOR(volatile_block_object);
        };
        typedef multi_index_container<
            volatile_block_object,
            indexed_by<
              ordered_unique< tag< by_id >,
                const_mem_fun< volatile_block_object, volatile_block_object::id_type, &volatile_block_object::get_id > >,
              ordered_unique< tag< by_block >,
                composite_key< volatile_block_object,
                  const_mem_fun< volatile_block_object, uint32_t, &volatile_block_object::get_block_number >,
                  const_mem_fun< volatile_block_object, volatile_block_object::id_type, &volatile_block_object::get_id >
                >
              >
            >,
            allocator< volatile_block_object >
          > volatile_block_index;

        class volatile_transaction_object : public object< volatile_block_object_type, volatile_transaction_object >
        {
          CHAINBASE_OBJECT( volatile_transaction_object );

          public:
            CHAINBASE_DEFAULT_CONSTRUCTOR(volatile_transaction_object)

            processing_objects::process_transaction_t transaction;

            uint32_t get_block_number() const
            {
              return transaction.block_number;
            }

            CHAINBASE_UNPACK_CONSTRUCTOR(volatile_transaction_object);
        };
        typedef multi_index_container<
            volatile_transaction_object,
            indexed_by<
              ordered_unique< tag< by_id >,
                const_mem_fun< volatile_transaction_object, volatile_transaction_object::id_type, &volatile_transaction_object::get_id > >,
              ordered_unique< tag< by_block >,
                composite_key< volatile_transaction_object,
                  const_mem_fun< volatile_transaction_object, uint32_t, &volatile_transaction_object::get_block_number >,
                  const_mem_fun< volatile_transaction_object, volatile_transaction_object::id_type, &volatile_transaction_object::get_id >
                >
              >
            >,
            allocator< volatile_transaction_object >
          > volatile_transaction_index;

        class volatile_transaction_multisig_object : public object< volatile_transaction_multisig_object_type, volatile_transaction_multisig_object >
        {
          CHAINBASE_OBJECT( volatile_transaction_multisig_object );

          public:
            CHAINBASE_DEFAULT_CONSTRUCTOR(volatile_transaction_multisig_object)

            processing_objects::process_transaction_multisig_block_t transaction_multisig;

            uint32_t get_block_number() const
            {
              return transaction_multisig.block_number;
            }

            CHAINBASE_UNPACK_CONSTRUCTOR(volatile_transaction_multisig_object);
        };
        typedef multi_index_container<
            volatile_transaction_multisig_object,
            indexed_by<
              ordered_unique< tag< by_id >,
                const_mem_fun< volatile_transaction_multisig_object, volatile_transaction_multisig_object::id_type, &volatile_transaction_multisig_object::get_id > >,
              ordered_unique< tag< by_block >,
                composite_key< volatile_transaction_multisig_object,
                  const_mem_fun< volatile_transaction_multisig_object, uint32_t, &volatile_transaction_multisig_object::get_block_number >,
                  const_mem_fun< volatile_transaction_multisig_object, volatile_transaction_multisig_object::id_type, &volatile_transaction_multisig_object::get_id >
                >
              >
            >,
            allocator< volatile_transaction_multisig_object >
          > volatile_transaction_multisig_index;

        class volatile_operation_object : public object< volatile_operation_object_type, volatile_operation_object >
        {
          CHAINBASE_OBJECT( volatile_operation_object );

          public:
            CHAINBASE_DEFAULT_CONSTRUCTOR(volatile_operation_object, (operation))

            processing_objects::volatile_process_operation_t operation;

            uint32_t get_block_number() const
            {
              return operation.block_number;
            }

            CHAINBASE_UNPACK_CONSTRUCTOR(volatile_operation_object, (operation));
        };
        typedef multi_index_container<
            volatile_operation_object,
            indexed_by<
              ordered_unique< tag< by_id >,
                const_mem_fun< volatile_operation_object, volatile_operation_object::id_type, &volatile_operation_object::get_id > >,
              ordered_unique< tag< by_block >,
                composite_key< volatile_operation_object,
                  const_mem_fun< volatile_operation_object, uint32_t, &volatile_operation_object::get_block_number >,
                  const_mem_fun< volatile_operation_object, volatile_operation_object::id_type, &volatile_operation_object::get_id >
                >
              >
            >,
            allocator< volatile_operation_object >
          > volatile_operation_index;

        class volatile_account_operation_object : public object< volatile_account_operation_object_type, volatile_account_operation_object >
        {
          CHAINBASE_OBJECT( volatile_account_operation_object );

          public:
            CHAINBASE_DEFAULT_CONSTRUCTOR(volatile_account_operation_object)

            processing_objects::account_operation_data_block_t account_operation;

            uint32_t get_block_number() const
            {
              return account_operation.block_number;
            }

            CHAINBASE_UNPACK_CONSTRUCTOR(volatile_account_operation_object);
        };
        typedef multi_index_container<
            volatile_account_operation_object,
            indexed_by<
              ordered_unique< tag< by_id >,
                const_mem_fun< volatile_account_operation_object, volatile_account_operation_object::id_type, &volatile_account_operation_object::get_id > >,
              ordered_unique< tag< by_block >,
                composite_key< volatile_account_operation_object,
                  const_mem_fun< volatile_account_operation_object, uint32_t, &volatile_account_operation_object::get_block_number >,
                  const_mem_fun< volatile_account_operation_object, volatile_account_operation_object::id_type, &volatile_account_operation_object::get_id >
                >
              >
            >,
            allocator< volatile_account_operation_object >
          > volatile_account_operation_index;
      } // namespace PSQL
    }    // namespace sql_serializer
  }      // namespace plugins
} // namespace hive

//===========base classes===========
FC_REFLECT( hive::plugins::sql_serializer::PSQL::processing_objects::process_base_t, (hash) )

FC_REFLECT_DERIVED( hive::plugins::sql_serializer::PSQL::processing_objects::process_base_ex_t, (hive::plugins::sql_serializer::PSQL::processing_objects::process_base_t),
(block_number) )

FC_REFLECT( hive::plugins::sql_serializer::PSQL::processing_objects::volatile_account_data_t, (id)(name) )

FC_REFLECT( hive::plugins::sql_serializer::PSQL::processing_objects::volatile_permlink_data_t, (id)(permlink) )

//===========SQL items===========
FC_REFLECT_DERIVED( hive::plugins::sql_serializer::PSQL::processing_objects::account_data_block_t, (hive::plugins::sql_serializer::PSQL::processing_objects::volatile_account_data_t),
(block_number) )

FC_REFLECT_DERIVED( hive::plugins::sql_serializer::PSQL::processing_objects::permlink_data_block_t, (hive::plugins::sql_serializer::PSQL::processing_objects::volatile_permlink_data_t),
(block_number) )

FC_REFLECT_DERIVED( hive::plugins::sql_serializer::PSQL::processing_objects::process_block_t, (hive::plugins::sql_serializer::PSQL::processing_objects::process_base_ex_t),
(created_at)(prev_hash) )

FC_REFLECT_DERIVED( hive::plugins::sql_serializer::PSQL::processing_objects::process_transaction_t, (hive::plugins::sql_serializer::PSQL::processing_objects::process_base_ex_t),
(trx_in_block)(ref_block_num)(ref_block_prefix)(expiration)(signature) )

FC_REFLECT_DERIVED( hive::plugins::sql_serializer::PSQL::processing_objects::process_transaction_multisig_t, (hive::plugins::sql_serializer::PSQL::processing_objects::process_base_t),
(signature) )

FC_REFLECT_DERIVED( hive::plugins::sql_serializer::PSQL::processing_objects::process_transaction_multisig_block_t, (hive::plugins::sql_serializer::PSQL::processing_objects::process_transaction_multisig_t),
(block_number) )

FC_REFLECT( hive::plugins::sql_serializer::PSQL::processing_objects::volatile_process_operation_t, (operation_id)(block_number)(trx_in_block)(op_in_trx)(op) )

FC_REFLECT( hive::plugins::sql_serializer::PSQL::processing_objects::account_operation_data_t, (operation_id)(account_id)(operation_seq_no) )

FC_REFLECT_DERIVED( hive::plugins::sql_serializer::PSQL::processing_objects::account_operation_data_block_t, (hive::plugins::sql_serializer::PSQL::processing_objects::account_operation_data_t),
(block_number) )
//===========volatile indexes===========
FC_REFLECT( hive::plugins::sql_serializer::PSQL::volatile_account_data_object,(id)(account_data))
FC_REFLECT( hive::plugins::sql_serializer::PSQL::volatile_permlink_data_object,(id)(permlink_data))
FC_REFLECT( hive::plugins::sql_serializer::PSQL::volatile_block_object,(id)(block))
FC_REFLECT( hive::plugins::sql_serializer::PSQL::volatile_transaction_object,(id)(transaction))
FC_REFLECT( hive::plugins::sql_serializer::PSQL::volatile_transaction_multisig_object,(id)(transaction_multisig))
FC_REFLECT( hive::plugins::sql_serializer::PSQL::volatile_operation_object,(id)(operation))
FC_REFLECT( hive::plugins::sql_serializer::PSQL::volatile_account_operation_object,(id)(account_operation))

CHAINBASE_SET_INDEX_TYPE( hive::plugins::sql_serializer::PSQL::volatile_account_data_object, hive::plugins::sql_serializer::PSQL::volatile_account_data_index )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::sql_serializer::PSQL::volatile_permlink_data_object, hive::plugins::sql_serializer::PSQL::volatile_permlink_data_index )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::sql_serializer::PSQL::volatile_block_object, hive::plugins::sql_serializer::PSQL::volatile_block_index )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::sql_serializer::PSQL::volatile_transaction_object, hive::plugins::sql_serializer::PSQL::volatile_transaction_index )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::sql_serializer::PSQL::volatile_transaction_multisig_object, hive::plugins::sql_serializer::PSQL::volatile_transaction_multisig_index )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::sql_serializer::PSQL::volatile_operation_object, hive::plugins::sql_serializer::PSQL::volatile_operation_index )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::sql_serializer::PSQL::volatile_account_operation_object, hive::plugins::sql_serializer::PSQL::volatile_account_operation_index )

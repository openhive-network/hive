#pragma once

#include <hive/chain/hive_object_types.hpp>

#include <boost/multi_index/composite_key.hpp>

#ifndef HIVE_ACCOUNT_HISTORY_ROCKSDB_SPACE_ID
#define HIVE_ACCOUNT_HISTORY_ROCKSDB_SPACE_ID 15
#endif

namespace hive { namespace plugins { namespace account_history_rocksdb {

using namespace hive::chain;

typedef std::vector<char> serialize_buffer_t;

enum account_history_rocksdb_object_types
{
  volatile_operation_object_type = ( HIVE_ACCOUNT_HISTORY_ROCKSDB_SPACE_ID << 8 )
};

class volatile_operation_object : public object< volatile_operation_object_type, volatile_operation_object >
{
  CHAINBASE_OBJECT( volatile_operation_object );

  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( volatile_operation_object, (serialized_op)(impacted) )

    chain::transaction_id_type trx_id;
    uint32_t                   block = 0;
    uint32_t                   trx_in_block = 0;
    uint32_t                   op_in_trx = 0;
    bool                       is_virtual = false;
    time_point_sec             timestamp;
    chain::buffer_type         serialized_op;
    chainbase::t_vector< account_name_type > impacted;
};

typedef oid_ref< volatile_operation_object > volatile_operation_id_type;

/** Dedicated definition is needed because of conflict of BIP allocator
  *  against usage of this class as temporary object.
  *  The conflict appears in original serialized_op container type definition,
  *  which in BIP version needs an allocator during constructor call.
  */
class rocksdb_operation_object
{
  public:
    rocksdb_operation_object() {}
    rocksdb_operation_object( const volatile_operation_object& o ) :
      trx_id( o.trx_id ),
      block( o.block ),
      trx_in_block( o.trx_in_block ),
      op_in_trx( o.op_in_trx ),
      is_virtual( o.is_virtual ),
      timestamp( o.timestamp )
    {
      serialized_op.insert( serialized_op.end(), o.serialized_op.begin(), o.serialized_op.end() );
    }

    uint64_t                    id = 0;

    chain::transaction_id_type trx_id;
    uint32_t                   block = 0;
    uint32_t                   trx_in_block = 0;
    uint32_t                   op_in_trx = 0;
    bool                       is_virtual = false;
    time_point_sec             timestamp;
    serialize_buffer_t         serialized_op;
};

struct by_block;

typedef multi_index_container<
    volatile_operation_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< volatile_operation_object, volatile_operation_object::id_type, &volatile_operation_object::get_id > >,
      ordered_unique< tag< by_block >,
        composite_key< volatile_operation_object,
          member< volatile_operation_object, uint32_t, &volatile_operation_object::block>,
          const_mem_fun< volatile_operation_object, volatile_operation_object::id_type, &volatile_operation_object::get_id >
        >
      >
    >,
    allocator< volatile_operation_object >
  > volatile_operation_index;

} } } // hive::plugins::account_history_rocksdb

FC_REFLECT( hive::plugins::account_history_rocksdb::volatile_operation_object, (id)(trx_id)(block)(trx_in_block)(op_in_trx)(is_virtual)(timestamp)(serialized_op)(impacted) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::account_history_rocksdb::volatile_operation_object, hive::plugins::account_history_rocksdb::volatile_operation_index )

FC_REFLECT( hive::plugins::account_history_rocksdb::rocksdb_operation_object, (id)(trx_id)(block)(trx_in_block)(op_in_trx)(is_virtual)(timestamp)(serialized_op) )

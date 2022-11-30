#pragma once
#include <hive/chain/hive_object_types.hpp>
#include <hive/protocol/transaction.hpp>

namespace hive { namespace plugins { namespace transaction_status {

using namespace hive::chain;

#ifndef HIVE_TRANSACTION_STATUS_SPACE_ID
#define HIVE_TRANSACTION_STATUS_SPACE_ID 18
#endif

enum transaction_status_object_type
{
  transaction_status_object_type = ( HIVE_TRANSACTION_STATUS_SPACE_ID << 8 )
};

enum transaction_status
{
  unknown,                           // Expiration time in future, transaction not included in block or mempool
  within_mempool,                    // Transaction in mempool
  within_reversible_block,           // Transaction has been included in block, block not irreversible
  within_irreversible_block,         // Transaction has been included in block, block is irreversible
  expired_reversible,                // Transaction has expired, transaction is not irreversible (transaction could be in a fork)
  expired_irreversible,              // Transaction has expired, transaction is irreversible (transaction cannot be in a fork)
  too_old                            // Transaction is too old, I don't know about it
};

class transaction_status_object : public object< transaction_status_object_type, transaction_status_object >
{
  CHAINBASE_OBJECT( transaction_status_object );

public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( transaction_status_object )

  transaction_id_type         transaction_id;
  int64_t                     rc_cost = -1;
  uint32_t                    block_num = 0;
};

typedef oid_ref< transaction_status_object > transaction_status_object_id_type;

struct by_trx_id;
struct by_block_num;
struct by_expiration;

typedef multi_index_container<
  transaction_status_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< transaction_status_object, transaction_status_object::id_type, &transaction_status_object::get_id > >,
    ordered_unique< tag< by_trx_id >,
      member< transaction_status_object, transaction_id_type, &transaction_status_object::transaction_id > >,
    ordered_unique< tag< by_block_num >,
      composite_key< transaction_status_object,
        member< transaction_status_object, uint32_t, &transaction_status_object::block_num >,
        member< transaction_status_object, transaction_id_type, &transaction_status_object::transaction_id >
      >,
      composite_key_compare< std::less< uint32_t >, std::less< transaction_id_type > >
    >
  >,
  allocator< transaction_status_object >
> transaction_status_index;



} } } // hive::plugins::transaction_status

FC_REFLECT_ENUM( hive::plugins::transaction_status::transaction_status,
            (unknown)
            (within_mempool)
            (within_reversible_block)
            (within_irreversible_block)
            (expired_reversible)
            (expired_irreversible)
            (too_old) )

FC_REFLECT( hive::plugins::transaction_status::transaction_status_object, (id)(transaction_id)(rc_cost)(block_num) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::transaction_status::transaction_status_object, hive::plugins::transaction_status::transaction_status_index )

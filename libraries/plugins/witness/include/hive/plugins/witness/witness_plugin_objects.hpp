#pragma once

#include <hive/chain/hive_object_types.hpp>

#ifndef HIVE_WITNESS_SPACE_ID
#define HIVE_WITNESS_SPACE_ID 19
#endif

namespace hive { namespace chain {
struct by_account;
} }

namespace hive { namespace plugins { namespace witness {

using namespace hive::chain;

enum witness_object_types
{
  witness_custom_op_object_type          = ( HIVE_WITNESS_SPACE_ID << 8 )
};

class witness_custom_op_object : public object< witness_custom_op_object_type, witness_custom_op_object >
{
  CHAINBASE_OBJECT( witness_custom_op_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( witness_custom_op_object )

    account_name_type    account;
    uint32_t             count = 0;
};

typedef multi_index_container<
  witness_custom_op_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< witness_custom_op_object, witness_custom_op_object::id_type, &witness_custom_op_object::get_id > >,
    ordered_unique< tag< by_account >,
      member< witness_custom_op_object, account_name_type, &witness_custom_op_object::account > >
  >,
  multi_index_allocator< witness_custom_op_object >
> witness_custom_op_index;

} } }

FC_REFLECT( hive::plugins::witness::witness_custom_op_object,
  (id)
  (account)
  (count)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::witness::witness_custom_op_object, hive::plugins::witness::witness_custom_op_index )

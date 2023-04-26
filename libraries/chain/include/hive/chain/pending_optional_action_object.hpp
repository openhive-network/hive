#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/required_automated_actions.hpp>

#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

using hive::protocol::optional_automated_action;

class pending_optional_action_object : public object< pending_optional_action_object_type, pending_optional_action_object >
{
  CHAINBASE_OBJECT( pending_optional_action_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( pending_optional_action_object )

    time_point_sec             execution_time;
    optional_automated_action  action;

  CHAINBASE_UNPACK_CONSTRUCTOR(pending_optional_action_object);
};

struct by_execution;

typedef multi_index_container<
  pending_optional_action_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< pending_optional_action_object, pending_optional_action_object::id_type, &pending_optional_action_object::get_id > >,
    ordered_unique< tag< by_execution >,
      composite_key< pending_optional_action_object,
        member< pending_optional_action_object, time_point_sec, &pending_optional_action_object::execution_time >,
        const_mem_fun< pending_optional_action_object, pending_optional_action_object::id_type, &pending_optional_action_object::get_id >
      >
    >
  >,
  allocator< pending_optional_action_object >
> pending_optional_action_index;

} } //hive::chain

FC_REFLECT( hive::chain::pending_optional_action_object,
        (id)(execution_time)(action) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::pending_optional_action_object, hive::chain::pending_optional_action_index )

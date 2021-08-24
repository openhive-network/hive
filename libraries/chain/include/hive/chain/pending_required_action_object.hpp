#pragma once
#include <chainbase/hive_fwd.hpp>

#include <hive/protocol/required_automated_actions.hpp>

#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

using hive::protocol::required_automated_action;

class pending_required_action_object : public object< pending_required_action_object_type, pending_required_action_object >
{
  CHAINBASE_OBJECT( pending_required_action_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( pending_required_action_object )

    time_point_sec             execution_time;
    required_automated_action  action;
  CHAINBASE_UNPACK_CONSTRUCTOR(pending_required_action_object);
};

struct by_execution;

typedef multi_index_container<
  pending_required_action_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< pending_required_action_object, pending_required_action_object::id_type, &pending_required_action_object::get_id > >,
    ordered_unique< tag< by_execution >,
      composite_key< pending_required_action_object,
        member< pending_required_action_object, time_point_sec, &pending_required_action_object::execution_time >,
        const_mem_fun< pending_required_action_object, pending_required_action_object::id_type, &pending_required_action_object::get_id >
      >
    >
  >,
  allocator< pending_required_action_object >
> pending_required_action_index;

} } //hive::chain

FC_REFLECT( hive::chain::pending_required_action_object,
        (id)(execution_time)(action) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::pending_required_action_object, hive::chain::pending_required_action_index )

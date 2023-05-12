#pragma once

#include <fc/int_array.hpp>
#include <fc/reflect/reflect.hpp>
#include <vector>

namespace hive { namespace protocol {

struct signed_transaction;

} } // hive::protocol

namespace hive { namespace chain {

class database;

enum rc_resource_types
{
  resource_history_bytes,
  resource_new_accounts,
  resource_market_bytes,
  resource_state_bytes,
  resource_execution_time
};

} } // hive::chain

FC_REFLECT_ENUM( hive::chain::rc_resource_types,
  (resource_history_bytes)
  (resource_new_accounts)
  (resource_market_bytes)
  (resource_state_bytes)
  (resource_execution_time)
)

namespace hive { namespace chain {

constexpr auto HIVE_RC_NUM_RESOURCE_TYPES = fc::reflector< rc_resource_types >::total_member_count;

typedef fc::int_array< int64_t, HIVE_RC_NUM_RESOURCE_TYPES > resource_count_type;
typedef fc::int_array< int64_t, HIVE_RC_NUM_RESOURCE_TYPES > resource_cost_type;
typedef fc::int_array< uint16_t, HIVE_RC_NUM_RESOURCE_TYPES > resource_share_type;

typedef resource_count_type count_resources_result;

// scans transaction for used resources
void count_resources(
  const hive::protocol::signed_transaction& tx,
  const size_t size,
  count_resources_result& result,
  const fc::time_point_sec now );

// scans single nonstandard operation for used resource (implemented for rc_custom_operation)
template< typename OpType >
void count_resources(
  const OpType& op,
  count_resources_result& result,
  const fc::time_point_sec now );

} } // hive::chain

FC_REFLECT_TYPENAME( hive::chain::resource_count_type )
FC_REFLECT_TYPENAME( hive::chain::resource_share_type )

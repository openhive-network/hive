#pragma once

#include <hive/protocol/transaction.hpp>

#include <fc/int_array.hpp>
#include <fc/reflect/reflect.hpp>
#include <vector>

namespace hive { namespace plugins { namespace rc {

enum rc_resource_types
{
  resource_history_bytes,
  resource_new_accounts,
  resource_market_bytes,
  resource_state_bytes,
  resource_execution_time
};

} } } // hive::plugins::rc

FC_REFLECT_ENUM( hive::plugins::rc::rc_resource_types,
  (resource_history_bytes)
  (resource_new_accounts)
  (resource_market_bytes)
  (resource_state_bytes)
  (resource_execution_time)
)

namespace hive { namespace plugins { namespace rc {

const auto HIVE_RC_NUM_RESOURCE_TYPES = fc::reflector< rc_resource_types >::total_member_count;

typedef fc::int_array< int64_t, HIVE_RC_NUM_RESOURCE_TYPES > resource_count_type;
typedef fc::int_array< int64_t, HIVE_RC_NUM_RESOURCE_TYPES > resource_cost_type;

struct count_resources_result
{
  resource_count_type resource_count;
};

void count_resources(
  const hive::protocol::signed_transaction& tx,
  count_resources_result& result
  );

} } } // hive::plugins::rc

FC_REFLECT( hive::plugins::rc::count_resources_result,
  (resource_count)
)

FC_REFLECT_TYPENAME( hive::plugins::rc::resource_count_type )

#pragma once

#include <hive/protocol/transaction.hpp>
#include <hive/chain/rc/resource_count.hpp>

#include <fc/int_array.hpp>
#include <fc/reflect/reflect.hpp>
#include <vector>

namespace hive { namespace plugins { namespace rc {

using hive::chain::rc_resource_types;
using hive::chain::HIVE_RC_NUM_RESOURCE_TYPES;
using hive::chain::resource_count_type;
using hive::chain::resource_cost_type;
using hive::chain::resource_share_type;

typedef resource_count_type count_resources_result;

void count_resources(
  const hive::protocol::signed_transaction& tx,
  const size_t size,
  count_resources_result& result,
  const fc::time_point_sec now
  );

} } } // hive::plugins::rc

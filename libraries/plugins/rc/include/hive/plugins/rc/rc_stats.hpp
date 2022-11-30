#pragma once

#include <hive/plugins/rc/resource_count.hpp>

#include <fc/int_array.hpp>
#include <fc/reflect/reflect.hpp>

namespace hive { namespace plugins { namespace rc {

constexpr auto HIVE_RC_NUM_OPERATIONS = hive::protocol::operation::tag< hive::protocol::fill_convert_request_operation >::value;
constexpr auto HIVE_RC_NUM_PAYER_RANKS = 8;

// statistics for transaction with single operation (or multiop transaction)
struct rc_op_stats
{
  resource_count_type usage;
  resource_cost_type  cost;
  uint32_t            count = 0;

  int64_t average_cost() const
  {
    if( count == 0 )
      return 0;
    int64_t result = 0;
    for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
      result += cost[i];
    return result / count;
  };
};

// statistics for payer with certain rank
struct rc_payer_stats
{
  resource_count_type usage;
  resource_cost_type  cost;

  uint32_t            less_than_5_percent = 0; // payer was observed with < 5% of his max RC
  uint32_t            less_than_10_percent = 0; // payer was observed with < 10% of his max RC
  uint32_t            less_than_20_percent = 0; // payer was observed with < 20% of his max RC
  fc::int_array< uint32_t, 3 >
                      cant_afford; // payer was observed with less than average cost in RC mana for first three ops (vote, comment, transfer)
  uint32_t            count = 0; // total number of times payer was observed

  bool was_dry() const
  {
    return cant_afford[0] || cant_afford[1] || cant_afford[2];
  }
};

} } } // hive::plugins::rc

FC_REFLECT( hive::plugins::rc::rc_op_stats,
  (usage)
  (cost)
  (count)
)

FC_REFLECT( hive::plugins::rc::rc_payer_stats,
  (usage)
  (cost)
  (less_than_5_percent)
  (less_than_10_percent)
  (less_than_20_percent)
  (cant_afford)
  (count)
)

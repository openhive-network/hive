#pragma once

#include <hive/protocol/types.hpp>

#include <hive/chain/rc/resource_count.hpp>

#include <fc/int_array.hpp>

#include <vector>

namespace hive { namespace chain {

using hive::protocol::account_name_type;

struct rc_info
{
  account_name_type         payer;
  resource_count_type       usage;
  resource_cost_type        cost;
  int64_t                   max = 0; //max rc of payer (can be zero when payer is unknown)
  int64_t                   rc = 0; //current rc mana of payer (can be zero when payer is unknown)
  optional< uint8_t >       op; //only filled when there is just one operation in tx
};

typedef rc_info rc_transaction_info;
typedef rc_info rc_optional_action_info;

struct rc_block_info
{
  resource_count_type       decay;
  optional< resource_count_type >
                            budget; //aside from pool of new accounts this is constant
  resource_count_type       usage;
  resource_count_type       pool;
  resource_cost_type        cost;
  fc::int_array< uint16_t, HIVE_RC_NUM_RESOURCE_TYPES >
                            share;
  int64_t                   regen = 0;
  optional< int64_t >       new_accounts_adjustment;
};

} } // hive::chain

FC_REFLECT( hive::chain::rc_info,
  (payer)
  (usage)
  (cost)
  (max)
  (rc)
  (op)
)

FC_REFLECT( hive::chain::rc_block_info,
  (decay)
  (budget)
  (usage)
  (pool)
  (cost)
  (share)
  (regen)
  (new_accounts_adjustment)
)

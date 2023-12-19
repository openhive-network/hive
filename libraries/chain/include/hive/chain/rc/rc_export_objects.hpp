#pragma once

#include <hive/protocol/types.hpp>

#include <hive/chain/rc/resource_count.hpp>

#include <fc/int_array.hpp>

#include <vector>

namespace hive { namespace chain {

using hive::protocol::account_name_type;

struct rc_transaction_info
{
  account_name_type         payer;
  resource_count_type       usage;
  resource_cost_type        cost;
  int64_t                   max = 0; //max rc of payer (can be zero when payer is unknown)
  int64_t                   rc = 0; //current rc mana of payer (can be zero when payer is unknown)
  optional< uint8_t >       op; //only filled when there is just one operation in tx
};

struct rc_block_info
{
  resource_count_type       usage;
  resource_cost_type        cost;
  uint32_t                  tx_count = 0; //number of transactions that accumulated usage/cost (grows as block is processed)
};

} } // hive::chain

FC_REFLECT( hive::chain::rc_transaction_info,
  (payer)
  (usage)
  (cost)
  (max)
  (rc)
  (op)
)

FC_REFLECT( hive::chain::rc_block_info,
  (usage)
  (cost)
  (tx_count)
)

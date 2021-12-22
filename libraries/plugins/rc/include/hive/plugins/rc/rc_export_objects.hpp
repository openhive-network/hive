#pragma once

#include <hive/protocol/types.hpp>

#include <hive/plugins/block_data_export/exportable_block_data.hpp>
#include <hive/plugins/rc/resource_count.hpp>

#include <fc/int_array.hpp>

#include <vector>

namespace hive { namespace plugins { namespace rc {

using hive::plugins::block_data_export::exportable_block_data;
using hive::protocol::account_name_type;

struct rc_info
{
  account_name_type         resource_user;
  count_resources_result    usage;
  resource_cost_type        cost;
};

typedef rc_info rc_transaction_info;
typedef rc_info rc_optional_action_info;

struct rc_block_info
{
  resource_count_type       decay;
  resource_count_type       budget; //aside from pool of new accounts this is constant
  resource_count_type       usage;
  resource_count_type       pool;
  fc::int_array< uint16_t, HIVE_RC_NUM_RESOURCE_TYPES >
                            pool_share;
  int64_t                   regen = 0;
  int64_t                   new_accounts_adjustment = 0;
};

struct exp_rc_data
  : public exportable_block_data
{
  exp_rc_data();
  virtual ~exp_rc_data();

  virtual void to_variant( fc::variant& v )const override;

  rc_block_info                          block_info;
  std::vector< rc_transaction_info >     tx_info;
  std::vector< rc_optional_action_info > opt_action_info;
};

} } } // hive::plugins::rc

FC_REFLECT( hive::plugins::rc::rc_transaction_info,
  (resource_user)
  (usage)
  (cost)
)

FC_REFLECT( hive::plugins::rc::rc_block_info,
  (decay)
  (budget)
  (usage)
  (pool)
  (pool_share)
  (regen)
  (new_accounts_adjustment)
)

FC_REFLECT( hive::plugins::rc::exp_rc_data,
  (block_info)
  (tx_info)
  (opt_action_info)
)

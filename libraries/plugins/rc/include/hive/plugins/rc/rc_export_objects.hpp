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

struct exp_rc_data
  : public exportable_block_data
{
  exp_rc_data();
  virtual ~exp_rc_data();

  virtual void to_variant( fc::variant& v )const override;

  void add_tx_info( const rc_transaction_info& tx_info )
  {
    if( !txs.valid() )
      txs = std::vector< rc_transaction_info >();
    txs->emplace_back( tx_info );
  }
  void add_opt_action_info( const rc_optional_action_info& action_info )
  {
    if( !opt_actions.valid() )
      opt_actions = std::vector< rc_optional_action_info >();
    opt_actions->emplace_back( action_info );
  }

  rc_block_info                                      block;
  optional< std::vector< rc_transaction_info > >     txs;
  optional< std::vector< rc_optional_action_info > > opt_actions;
};

} } } // hive::plugins::rc

FC_REFLECT( hive::plugins::rc::rc_info,
  (payer)
  (usage)
  (cost)
  (max)
  (rc)
  (op)
)

FC_REFLECT( hive::plugins::rc::rc_block_info,
  (decay)
  (budget)
  (usage)
  (pool)
  (cost)
  (share)
  (regen)
  (new_accounts_adjustment)
)

FC_REFLECT( hive::plugins::rc::exp_rc_data,
  (block)
  (txs)
  (opt_actions)
)

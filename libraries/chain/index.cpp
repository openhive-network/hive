
#include <hive/chain/hive_object_types.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/history_object.hpp>
#include <hive/chain/pending_required_action_object.hpp>
#include <hive/chain/pending_optional_action_object.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/sps_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/witness_schedule.hpp>

namespace hive { namespace chain {

void initialize_core_indexes( database& db )
{
  HIVE_ADD_CORE_INDEX(db, dynamic_global_property_index);
  HIVE_ADD_CORE_INDEX(db, account_index);
  HIVE_ADD_CORE_INDEX(db, account_metadata_index);
  HIVE_ADD_CORE_INDEX(db, account_authority_index);
  HIVE_ADD_CORE_INDEX(db, witness_index);
  HIVE_ADD_CORE_INDEX(db, transaction_index);
  HIVE_ADD_CORE_INDEX(db, block_summary_index);
  HIVE_ADD_CORE_INDEX(db, witness_schedule_index);
  HIVE_ADD_CORE_INDEX(db, comment_index);
  HIVE_ADD_CORE_INDEX(db, comment_vote_index);
  HIVE_ADD_CORE_INDEX(db, witness_vote_index);
  HIVE_ADD_CORE_INDEX(db, limit_order_index);
  HIVE_ADD_CORE_INDEX(db, feed_history_index);
  HIVE_ADD_CORE_INDEX(db, convert_request_index);
  HIVE_ADD_CORE_INDEX(db, collateralized_convert_request_index);
  HIVE_ADD_CORE_INDEX(db, liquidity_reward_balance_index);
  HIVE_ADD_CORE_INDEX(db, operation_index);
  HIVE_ADD_CORE_INDEX(db, account_history_index);
  HIVE_ADD_CORE_INDEX(db, hardfork_property_index);
  HIVE_ADD_CORE_INDEX(db, withdraw_vesting_route_index);
  HIVE_ADD_CORE_INDEX(db, owner_authority_history_index);
  HIVE_ADD_CORE_INDEX(db, account_recovery_request_index);
  HIVE_ADD_CORE_INDEX(db, change_recovery_account_request_index);
  HIVE_ADD_CORE_INDEX(db, escrow_index);
  HIVE_ADD_CORE_INDEX(db, savings_withdraw_index);
  HIVE_ADD_CORE_INDEX(db, decline_voting_rights_request_index);
  HIVE_ADD_CORE_INDEX(db, reward_fund_index);
  HIVE_ADD_CORE_INDEX(db, vesting_delegation_index);
  HIVE_ADD_CORE_INDEX(db, vesting_delegation_expiration_index);
  HIVE_ADD_CORE_INDEX(db, pending_required_action_index);
  HIVE_ADD_CORE_INDEX(db, pending_optional_action_index);
#ifdef HIVE_ENABLE_SMT
  HIVE_ADD_CORE_INDEX(db, smt_token_index);
  HIVE_ADD_CORE_INDEX(db, account_regular_balance_index);
  HIVE_ADD_CORE_INDEX(db, account_rewards_balance_index);
  HIVE_ADD_CORE_INDEX(db, nai_pool_index);
  HIVE_ADD_CORE_INDEX(db, smt_token_emissions_index);
  HIVE_ADD_CORE_INDEX(db, smt_contribution_index);
  HIVE_ADD_CORE_INDEX(db, smt_ico_index);
#endif
  HIVE_ADD_CORE_INDEX(db, proposal_index);
  HIVE_ADD_CORE_INDEX(db, proposal_vote_index);
  HIVE_ADD_CORE_INDEX(db, comment_cashout_index);
  HIVE_ADD_CORE_INDEX(db, comment_cashout_ex_index);
  HIVE_ADD_CORE_INDEX(db, recurrent_transfer_index);
}

index_info::index_info() {}
index_info::~index_info() {}

} }

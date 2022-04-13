#pragma once

#include <hive/protocol/hive_operations.hpp>

#include <hive/chain/evaluator.hpp>

namespace hive { namespace chain {

using namespace hive::protocol;

HIVE_DEFINE_EVALUATOR( account_create )
HIVE_DEFINE_EVALUATOR( account_create_with_delegation )
HIVE_DEFINE_EVALUATOR( account_update )
HIVE_DEFINE_EVALUATOR( account_update2 )
HIVE_DEFINE_EVALUATOR( transfer )
HIVE_DEFINE_EVALUATOR( transfer_to_vesting )
HIVE_DEFINE_EVALUATOR( witness_update )
HIVE_DEFINE_EVALUATOR( account_witness_vote )
HIVE_DEFINE_EVALUATOR( account_witness_proxy )
HIVE_DEFINE_EVALUATOR( withdraw_vesting )
HIVE_DEFINE_EVALUATOR( set_withdraw_vesting_route )
HIVE_DEFINE_EVALUATOR( comment )
HIVE_DEFINE_EVALUATOR( comment_options )
HIVE_DEFINE_EVALUATOR( delete_comment )
HIVE_DEFINE_EVALUATOR( vote )
HIVE_DEFINE_EVALUATOR( custom )
HIVE_DEFINE_EVALUATOR( custom_json )
HIVE_DEFINE_EVALUATOR( custom_binary )
HIVE_DEFINE_EVALUATOR( pow )
HIVE_DEFINE_EVALUATOR( pow2 )
HIVE_DEFINE_EVALUATOR( feed_publish )
HIVE_DEFINE_EVALUATOR( convert )
HIVE_DEFINE_EVALUATOR( collateralized_convert )
HIVE_DEFINE_EVALUATOR( limit_order_create )
HIVE_DEFINE_EVALUATOR( limit_order_cancel )
HIVE_DEFINE_EVALUATOR( report_over_production )
HIVE_DEFINE_EVALUATOR( limit_order_create2 )
HIVE_DEFINE_EVALUATOR( escrow_transfer )
HIVE_DEFINE_EVALUATOR( escrow_approve )
HIVE_DEFINE_EVALUATOR( escrow_dispute )
HIVE_DEFINE_EVALUATOR( escrow_release )
HIVE_DEFINE_EVALUATOR( claim_account )
HIVE_DEFINE_EVALUATOR( create_claimed_account )
HIVE_DEFINE_EVALUATOR( request_account_recovery )
HIVE_DEFINE_EVALUATOR( recover_account )
HIVE_DEFINE_EVALUATOR( change_recovery_account )
HIVE_DEFINE_EVALUATOR( transfer_to_savings )
HIVE_DEFINE_EVALUATOR( transfer_from_savings )
HIVE_DEFINE_EVALUATOR( cancel_transfer_from_savings )
HIVE_DEFINE_EVALUATOR( decline_voting_rights )
HIVE_DEFINE_EVALUATOR( reset_account )
HIVE_DEFINE_EVALUATOR( set_reset_account )
HIVE_DEFINE_EVALUATOR( claim_reward_balance )
#ifdef HIVE_ENABLE_SMT
HIVE_DEFINE_EVALUATOR( claim_reward_balance2 )
#endif
HIVE_DEFINE_EVALUATOR( delegate_vesting_shares )
HIVE_DEFINE_EVALUATOR( witness_set_properties )
#ifdef HIVE_ENABLE_SMT
HIVE_DEFINE_EVALUATOR( smt_setup )
HIVE_DEFINE_EVALUATOR( smt_setup_emissions )
HIVE_DEFINE_EVALUATOR( smt_set_setup_parameters )
HIVE_DEFINE_EVALUATOR( smt_set_runtime_parameters )
HIVE_DEFINE_EVALUATOR( smt_create )
HIVE_DEFINE_EVALUATOR( smt_contribute )
#endif
HIVE_DEFINE_EVALUATOR( create_proposal )
HIVE_DEFINE_EVALUATOR(update_proposal)
HIVE_DEFINE_EVALUATOR( update_proposal_votes )
HIVE_DEFINE_EVALUATOR( remove_proposal )
HIVE_DEFINE_EVALUATOR( recurrent_transfer )
HIVE_DEFINE_EVALUATOR( witness_block_approve )

} } // hive::chain

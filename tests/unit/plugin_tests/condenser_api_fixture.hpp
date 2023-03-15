#pragma once

#if defined IS_TEST_NET

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::plugins;

namespace hive { namespace plugins { 

namespace condenser_api {
  class condenser_api;
}

namespace account_history {
  class account_history_api;
}

} } // hive::plugins  

struct condenser_api_fixture : database_fixture
{
  condenser_api_fixture();
  virtual ~condenser_api_fixture();

  hive::plugins::condenser_api::condenser_api* condenser_api = nullptr;
  hive::plugins::account_history::account_history_api* account_history_api = nullptr;

  // Below are testing scenarios that trigger pushing different operations in the block(s).
  // Each scenario is intended to be used by several tests (e.g. tests on get_ops_in_block/get_transaction/get_account_history)

  typedef std::function < void( uint32_t generate_no_further_than ) > check_point_tester_t;

  /** 
   * Tests the operations that happen only on hardfork 1 - vesting_shares_split_operation & system_warning_operation (block 21)
   * Also tests: hardfork_operation & producer_reward_operation
   * 
   * The operations happen in block 2 (vesting_shares_split_operation, when HF1 is set) and block 21 (system_warning_operation),
   * regardless of configurations settings of the fixture.
  */
  void hf1_scenario( check_point_tester_t check_point_tester );

  /** 
   * Tests pow_operation that needs hardfork lower than 13.
   * Also tests: pow_reward_operation, account_created_operation, comment_operation (see database_fixture::create_with_pow) & producer_reward_operation.
   * 
   * All tested operations happen in block 3 (when create_with_pow is called) regardless of configurations settings of the fixture.
  */
  void hf12_scenario( check_point_tester_t check_point_tester );

  /**
   * Tests operations that need hardfork lower than 20:
   *  pow2_operation (< hf17), ineffective_delete_comment_operation (< hf19) & account_create_with_delegation_operation (< hf20)
   * Also tests: 
   *  account_created_operation, pow_reward_operation, transfer_operation, comment_operation, comment_options_operation,
   *  vote_operation, effective_comment_vote_operation, delete_comment_operation & producer_reward_operation
   */
  void hf13_scenario( check_point_tester_t check_point_1_tester, check_point_tester_t check_point_2_tester );

  /**
   * Operations tested here:
   *  curation_reward_operation, author_reward_operation, comment_reward_operation, comment_payout_update_operation,
   *  claim_reward_balance_operation, producer_reward_operation
   */  
  void comment_and_reward_scenario( check_point_tester_t check_point_1_tester, check_point_tester_t check_point_2_tester );

  /**
   * Operations tested here:
   *  convert_operation, collateralized_convert_operation, collateralized_convert_immediate_conversion_operation,
   *  limit_order_create_operation, limit_order_create2_operation, limit_order_cancel_operation, limit_order_cancelled_operation,
   *  producer_reward_operation,
   *  fill_convert_request_operation, fill_collateralized_convert_request_operation
   */  
  void convert_and_limit_order_scenario( check_point_tester_t check_point_tester );

  /**
   * Operations tested here:
   *  transfer_to_vesting_operation, transfer_to_vesting_completed_operation, set_withdraw_vesting_route_operation,
   *  delegate_vesting_shares_operation, withdraw_vesting_operation, producer_reward_operation,
   *  fill_vesting_withdraw_operation & return_vesting_delegation_operation
   */
  void vesting_scenario( check_point_tester_t check_point_tester );

  /**
   * Operations tested here:
   *  witness_update_operation, feed_publish_operation, account_witness_proxy_operation, account_witness_vote_operation, witness_set_properties_operation
   * Also tested here: 
   *  producer_reward_operation
   *  
   * Note that witness_block_approve_operation never appears in block (see its evaluator).
   */
  void witness_scenario( check_point_tester_t check_point_tester );

  /**
   * Operations tested here:
   *  escrow_transfer_operation, escrow_approve_operation, escrow_approved_operation, escrow_release_operation, escrow_dispute_operation,
   *  transfer_to_savings_operation, transfer_from_savings_operation, cancel_transfer_from_savings_operation & fill_transfer_from_savings_operation
   */
  void escrow_and_savings_scenario( check_point_tester_t check_point_tester );

  /**
   * Operations tested here:
   *  dhf_funding_operation, dhf_conversion_operation, transfer_operation,
   *  create_proposal_operation, proposal_fee_operation, update_proposal_operation, update_proposal_votes_operation & remove_proposal_operation
   */
  void proposal_scenario( check_point_tester_t check_point_tester );

  /**
   * Operations tested here:
   *  claim_account_operation, create_claimed_account_operation,
   *  change_recovery_account_operation, changed_recovery_account_operation,
   *  request_account_recovery_operation, recover_account_operation,
   *  account_update_operation & account_update2_operation
   * 
   * Note that reset_account_operation & set_reset_account_operation have been disabled and do not occur in blockchain.
   */
  void account_scenario( check_point_tester_t check_point_tester );

  /**
   * Operations tested here:
   *  custom_operation, custom_json_operation & producer_reward_operation
   * 
   * Note that custom_binary_operation has been disabled and does not occur in blockchain.
   */
  void custom_scenario( check_point_tester_t check_point_tester );

  /**
   * Operations tested here:
   *  recurrent_transfer_operation, fill_recurrent_transfer_operation, failed_recurrent_transfer_operation
   *  & producer_reward_operation
   */
  void recurrent_transfer_scenario( check_point_tester_t check_point_tester );

  /**
   * Operations tested here:
   *  decline_voting_rights_operation, declined_voting_rights_operation & producer_reward_operation
   */
  void decline_voting_rights_scenario( check_point_tester_t check_point_tester );

};

#endif

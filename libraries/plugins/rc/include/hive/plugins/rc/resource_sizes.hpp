#pragma once

#include <fc/reflect/reflect.hpp>

// This header defines the resource sizes.  It is an implementation detail of resource_count.cpp,
// but you may want it if you are re-implementing resource_count.cpp in a client library.

namespace hive { namespace plugins { namespace rc {

struct state_object_size_info
{
  state_object_size_info();

  const int64_t TEMPORARY_STATE_BYTE;
  const int64_t LASTING_STATE_BYTE;
  const int64_t PERSISTENT_STATE_BYTE;

  // account create (all versions)
  const int64_t account_create_base_size;
  const int64_t account_json_metadata_char_size;
  const int64_t authority_account_member_size;
  const int64_t authority_key_member_size;

  // account update (both versions) and nonsubsidized account recovery
  const int64_t owner_authority_history_object_size;

  // transfer to vesting
  const int64_t transfer_to_vesting_size;

  // request account recovery
  const int64_t request_account_recovery_size;

  // change recovery account
  const int64_t change_recovery_account_size;

  // comment
  const int64_t comment_base_size;
  const int64_t comment_permlink_char_size;
  const int64_t comment_beneficiaries_member_size;

  // vote
  const int64_t vote_size;

  // convert
  const int64_t convert_size;
  // collateralized convert
  const int64_t collateralized_convert_size;

  // decline voting rights
  const int64_t decline_voting_rights_size;

  // escrow transfer
  const int64_t escrow_transfer_size;

  // limit order create (both versions)
  const int64_t limit_order_create_size;

  // transfer from savings
  const int64_t transfer_from_savings_size;

  // transaction
  const int64_t transaction_base_size;
  const int64_t transaction_byte_size;

  // delegate vesting shares
  const int64_t vesting_delegation_object_size;
  const int64_t vesting_delegation_expiration_object_size;
  const int64_t delegate_vesting_shares_size;

  // set withdraw vesting route
  const int64_t set_withdraw_vesting_route_size;

  // witness update
  const int64_t witness_update_base_size;
  const int64_t witness_update_url_char_size;

  // account witness vote
  const int64_t account_witness_vote_size;

  // create proposal
  const int64_t create_proposal_base_size; //multiply by actual lifetime
  const int64_t create_proposal_subject_permlink_char_size; //multiply by actual lifetime

  // update proposal votes
  const int64_t update_proposal_votes_member_size;
  
  // recurrent transfer
  const int64_t recurrent_transfer_base_size; //multiply by actual lifetime

};

struct operation_exec_info
{
  operation_exec_info();

  const int64_t account_create_time;
  const int64_t account_create_with_delegation_time;
  const int64_t account_witness_vote_time;
  const int64_t comment_time;
  const int64_t comment_options_time;
  const int64_t convert_time;
  const int64_t collateralized_convert_time;
  const int64_t create_claimed_account_time;
  const int64_t decline_voting_rights_time;
  const int64_t delegate_vesting_shares_time;
  const int64_t escrow_transfer_time;
  const int64_t limit_order_create_time;
  const int64_t limit_order_create2_time;
  const int64_t request_account_recovery_time;
  const int64_t set_withdraw_vesting_route_time;
  const int64_t vote_time;
  const int64_t witness_update_time;
  const int64_t transfer_time;
  const int64_t transfer_to_vesting_time;
  const int64_t transfer_to_savings_time;
  const int64_t transfer_from_savings_time;
  const int64_t claim_reward_balance_time;
  const int64_t withdraw_vesting_time;
  const int64_t account_update_time;
  const int64_t account_update2_time;
  const int64_t account_witness_proxy_time;
  const int64_t cancel_transfer_from_savings_time;
  const int64_t change_recovery_account_time;
  const int64_t claim_account_time;
  const int64_t custom_time;
  const int64_t custom_json_time;
  const int64_t custom_binary_time;
  const int64_t delete_comment_time;
  const int64_t escrow_approve_time;
  const int64_t escrow_dispute_time;
  const int64_t escrow_release_time;
  const int64_t feed_publish_time;
  const int64_t limit_order_cancel_time;
  const int64_t witness_set_properties_time;
#ifdef HIVE_ENABLE_SMT
  const int64_t claim_reward_balance2_time;
  const int64_t smt_setup_time;
  const int64_t smt_setup_emissions_time;
  const int64_t smt_set_setup_parameters_time;
  const int64_t smt_set_runtime_parameters_time;
  const int64_t smt_create_time;
  const int64_t smt_contribute_time;
#endif
  const int64_t create_proposal_time;
  const int64_t update_proposal_time;
  const int64_t update_proposal_votes_time;
  const int64_t remove_proposal_time;
  const int64_t recurrent_transfer_base_time; //processing separately below
  const int64_t recurrent_transfer_processing_time; //multiply by number of transfers

  const int64_t verify_authority_time; //multiply by number of signatures
  const int64_t transaction_time;

  //not used but included since we have measurements
  const int64_t recover_account_time;
  const int64_t pow_time;
  const int64_t pow2_time;

};

} } }

FC_REFLECT( hive::plugins::rc::state_object_size_info,
  ( TEMPORARY_STATE_BYTE )
  ( LASTING_STATE_BYTE )
  ( PERSISTENT_STATE_BYTE )
  ( account_create_base_size )
  ( account_json_metadata_char_size )
  ( authority_account_member_size )
  ( authority_key_member_size )
  ( owner_authority_history_object_size )
  ( transfer_to_vesting_size )
  ( request_account_recovery_size )
  ( change_recovery_account_size )
  ( comment_base_size )
  ( comment_permlink_char_size )
  ( comment_beneficiaries_member_size )
  ( vote_size )
  ( convert_size )
  ( collateralized_convert_size )
  ( decline_voting_rights_size )
  ( escrow_transfer_size )
  ( limit_order_create_size )
  ( transfer_from_savings_size )
  ( transaction_base_size )
  ( transaction_byte_size )
  ( vesting_delegation_object_size )
  ( vesting_delegation_expiration_object_size )
  ( delegate_vesting_shares_size )
  ( set_withdraw_vesting_route_size )
  ( witness_update_base_size )
  ( witness_update_url_char_size )
  ( account_witness_vote_size )
  ( create_proposal_base_size )
  ( create_proposal_subject_permlink_char_size )
  ( update_proposal_votes_member_size )
  ( recurrent_transfer_base_size )
  )

FC_REFLECT( hive::plugins::rc::operation_exec_info,
  ( account_create_time )
  ( account_create_with_delegation_time )
  ( account_witness_vote_time )
  ( comment_time )
  ( comment_options_time )
  ( convert_time )
  ( collateralized_convert_time )
  ( create_claimed_account_time )
  ( decline_voting_rights_time )
  ( delegate_vesting_shares_time )
  ( escrow_transfer_time )
  ( limit_order_create_time )
  ( limit_order_create2_time )
  ( request_account_recovery_time )
  ( set_withdraw_vesting_route_time )
  ( vote_time )
  ( witness_update_time )
  ( transfer_time )
  ( transfer_to_vesting_time )
  ( transfer_to_savings_time )
  ( transfer_from_savings_time )
  ( claim_reward_balance_time )
  ( withdraw_vesting_time )
  ( account_update_time )
  ( account_update2_time )
  ( account_witness_proxy_time )
  ( cancel_transfer_from_savings_time )
  ( change_recovery_account_time )
  ( claim_account_time )
  ( custom_time )
  ( custom_json_time )
  ( custom_binary_time )
  ( delete_comment_time )
  ( escrow_approve_time )
  ( escrow_dispute_time )
  ( escrow_release_time )
  ( feed_publish_time )
  ( limit_order_cancel_time )
  ( witness_set_properties_time )
#ifdef HIVE_ENABLE_SMT
  ( claim_reward_balance2_time )
  ( smt_setup_time )
  ( smt_setup_emissions_time )
  ( smt_set_setup_parameters_time )
  ( smt_set_runtime_parameters_time )
  ( smt_create_time )
  ( smt_contribute_time )
#endif
  ( create_proposal_time )
  ( update_proposal_time )
  ( update_proposal_votes_time )
  ( remove_proposal_time )
  ( recurrent_transfer_base_time )
  ( recurrent_transfer_processing_time )
  ( verify_authority_time )
  ( transaction_time )
  ( recover_account_time )
  ( pow_time )
  ( pow2_time )
  )

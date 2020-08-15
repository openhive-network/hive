#pragma once

#include <hive/protocol/types.hpp>

#include <hive/protocol/operation_util.hpp>
#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/hive_virtual_operations.hpp>
#include <hive/protocol/smt_operations.hpp>
#include <hive/protocol/sps_operations.hpp>

namespace hive { namespace protocol {

  /** NOTE: do not change the order of any operations prior to the virtual operations
    * or it will trigger a hardfork.
    */
  typedef fc::static_variant<
        vote_operation,
        comment_operation,

        transfer_operation,
        transfer_to_vesting_operation,
        withdraw_vesting_operation,

        limit_order_create_operation,
        limit_order_cancel_operation,

        feed_publish_operation,
        convert_operation,

        account_create_operation,
        account_update_operation,

        witness_update_operation,
        account_witness_vote_operation,
        account_witness_proxy_operation,

        pow_operation,

        custom_operation,

        report_over_production_operation,

        delete_comment_operation,
        custom_json_operation,
        comment_options_operation,
        set_withdraw_vesting_route_operation,
        limit_order_create2_operation,
        claim_account_operation,
        create_claimed_account_operation,
        request_account_recovery_operation,
        recover_account_operation,
        change_recovery_account_operation,
        escrow_transfer_operation,
        escrow_dispute_operation,
        escrow_release_operation,
        pow2_operation,
        escrow_approve_operation,
        transfer_to_savings_operation,
        transfer_from_savings_operation,
        cancel_transfer_from_savings_operation,
        custom_binary_operation,
        decline_voting_rights_operation,
        reset_account_operation,
        set_reset_account_operation,
        claim_reward_balance_operation,
        delegate_vesting_shares_operation,
        account_create_with_delegation_operation,
        witness_set_properties_operation,
        account_update2_operation,
        create_proposal_operation,
        update_proposal_votes_operation,
        remove_proposal_operation,
        update_proposal_operation,

#ifdef HIVE_ENABLE_SMT
        /// SMT operations
        claim_reward_balance2_operation,

        smt_setup_operation,
        smt_setup_emissions_operation,
        smt_set_setup_parameters_operation,
        smt_set_runtime_parameters_operation,
        smt_create_operation,
        smt_contribute_operation,
#endif

        /// virtual operations below this point
        fill_convert_request_operation,
        author_reward_operation,
        curation_reward_operation,
        comment_reward_operation,
        liquidity_reward_operation,
        interest_operation,
        fill_vesting_withdraw_operation,
        fill_order_operation,
        shutdown_witness_operation,
        fill_transfer_from_savings_operation,
        hardfork_operation,
        comment_payout_update_operation,
        return_vesting_delegation_operation,
        comment_benefactor_reward_operation,
        producer_reward_operation,
        clear_null_account_balance_operation,
        proposal_pay_operation,
        sps_fund_operation,
        sps_convert_operation,
        hardfork_hive_operation,
        hardfork_hive_restore_operation,
        delayed_voting_operation,
        consolidate_treasury_balance_operation,
        effective_comment_vote_operation,
        ineffective_delete_comment_operation
      > operation;

  /*void operation_get_required_authorities( const operation& op,
                              flat_set<string>& active,
                              flat_set<string>& owner,
                              flat_set<string>& posting,
                              vector<authority>&  other );

  void operation_validate( const operation& op );*/

  bool is_market_operation( const operation& op );

  bool is_virtual_operation( const operation& op );

} } // hive::protocol

/*namespace fc {
    void to_variant( const hive::protocol::operation& var,  fc::variant& vo );
    void from_variant( const fc::variant& var,  hive::protocol::operation& vo );
}*/

HIVE_DECLARE_OPERATION_TYPE( hive::protocol::operation )
FC_REFLECT_TYPENAME( hive::protocol::operation )

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
        vote_operation, // 0
        comment_operation, // 1

        transfer_operation, // 2
        transfer_to_vesting_operation, // 3
        withdraw_vesting_operation, // 4

        limit_order_create_operation, // 5
        limit_order_cancel_operation, // 6

        feed_publish_operation, // 7
        convert_operation, // 8

        account_create_operation, // 9
        account_update_operation, // 10

        witness_update_operation, // 11
        account_witness_vote_operation, // 12
        account_witness_proxy_operation, // 13

        pow_operation, // 14

        custom_operation, // 15

        report_over_production_operation, // 16

        delete_comment_operation, // 17
        custom_json_operation, // 18
        comment_options_operation, // 19
        set_withdraw_vesting_route_operation, // 20
        limit_order_create2_operation, // 21
        claim_account_operation, // 22
        create_claimed_account_operation, // 23
        request_account_recovery_operation, // 24
        recover_account_operation, // 25
        change_recovery_account_operation, // 26
        escrow_transfer_operation, // 27
        escrow_dispute_operation, // 28
        escrow_release_operation, // 29
        pow2_operation, // 30
        escrow_approve_operation, // 31
        transfer_to_savings_operation, // 32
        transfer_from_savings_operation, // 33
        cancel_transfer_from_savings_operation, // 34
        custom_binary_operation, // 35
        decline_voting_rights_operation, // 36
        reset_account_operation, // 37
        set_reset_account_operation, // 38
        claim_reward_balance_operation, // 39
        delegate_vesting_shares_operation, // 40
        account_create_with_delegation_operation, // 41
        witness_set_properties_operation, // 42
        account_update2_operation, // 43
        create_proposal_operation, // 44
        update_proposal_votes_operation, // 45
        remove_proposal_operation, // 46
        update_proposal_operation, // 47
        collateralized_convert_operation, // 48
        recurrent_transfer_operation, // 49

#ifdef HIVE_ENABLE_SMT
        /// SMT operations
        claim_reward_balance2_operation, // last_pre_smt + 1

        smt_setup_operation, // last_pre_smt + 2
        smt_setup_emissions_operation, // last_pre_smt + 3
        smt_set_setup_parameters_operation, // last_pre_smt + 4
        smt_set_runtime_parameters_operation, // last_pre_smt + 5
        smt_create_operation, // last_pre_smt + 5
        smt_contribute_operation, // last_pre_smt + 6
#endif

        /// virtual operations below this point
        fill_convert_request_operation, // last_regular + 1
        author_reward_operation, // last_regular + 2
        curation_reward_operation, // last_regular + 3
        comment_reward_operation, // last_regular + 4
        liquidity_reward_operation, // last_regular + 5
        interest_operation, // last_regular + 6
        fill_vesting_withdraw_operation, // last_regular + 7
        fill_order_operation, // last_regular + 8
        shutdown_witness_operation, // last_regular + 9
        fill_transfer_from_savings_operation, // last_regular + 10
        hardfork_operation, // last_regular + 11
        comment_payout_update_operation, // last_regular + 12
        return_vesting_delegation_operation, // last_regular + 13
        comment_benefactor_reward_operation, // last_regular + 14
        producer_reward_operation, // last_regular + 15
        clear_null_account_balance_operation, // last_regular + 16
        proposal_pay_operation, // last_regular + 17
        sps_fund_operation, // last_regular + 18
        hardfork_hive_operation, // last_regular + 19
        hardfork_hive_restore_operation, // last_regular + 20
        delayed_voting_operation, // last_regular + 21
        consolidate_treasury_balance_operation, // last_regular + 22
        effective_comment_vote_operation, // last_regular + 23
        ineffective_delete_comment_operation, // last_regular + 24
        sps_convert_operation, // last_regular + 25
        expired_account_notification_operation, // last_regular + 26
        changed_recovery_account_operation, // last_regular + 27
        transfer_to_vesting_completed_operation, // last_regular + 28
        pow_reward_operation, // last_regular + 29
        vesting_shares_split_operation, // last_regular + 30
        account_created_operation, // last_regular + 31
        fill_collateralized_convert_request_operation, // last_regular + 32
        system_warning_operation, // last_regular + 33,
        fill_recurrent_transfer_operation, // last_regular + 34
        failed_recurrent_transfer_operation, // last_regular + 35
        limit_order_cancelled_operation,  // last_regular + 36
        producer_missed_operation // last_regular + 37
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

namespace fc
{
  using hive::protocol::operation;
  template<>
  struct serialization_functor< operation >
  {
    bool operator()( const fc::variant& v, operation& s ) const
    {
      return extended_serialization_functor< operation >().serialize( v, s );
    }
  };

  template<>
  struct variant_creator_functor< operation >
  {
    template<typename T>
    fc::variant operator()( const T& v ) const
    {
      return extended_variant_creator_functor< operation >().create( v );
    }
  };
}
/*namespace fc {
    void to_variant( const hive::protocol::operation& var,  fc::variant& vo );
    void from_variant( const fc::variant& var,  hive::protocol::operation& vo );
}*/

HIVE_DECLARE_OPERATION_TYPE( hive::protocol::operation )
FC_REFLECT_TYPENAME( hive::protocol::operation )

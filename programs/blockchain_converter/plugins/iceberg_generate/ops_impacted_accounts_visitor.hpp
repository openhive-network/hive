#pragma once

#include <boost/container/flat_set.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/forward_impacted.hpp>

namespace hive { namespace converter { namespace plugins { namespace iceberg_generate { namespace detail {

  /**
   * @brief After visiting, returns map of accounts and their required minimum balances to successfully evaluate
   */
  class ops_impacted_accounts_visitor
  {
  private:
    boost::container::flat_set<hive::protocol::account_name_type>& accounts_new;
    boost::container::flat_set<hive::protocol::account_name_type>& accounts_created;
    blockchain_converter& converter;

  public:
    typedef void result_type;

    ops_impacted_accounts_visitor(
      boost::container::flat_set<account_name_type>& accounts_new,
      boost::container::flat_set<account_name_type>& accounts_created,
      blockchain_converter& converter
    )
      : accounts_new( accounts_new ), accounts_created( accounts_created ), converter( converter )
    {}

    template< typename T >
    result_type operator()( const T& op )const
    {
      FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );

      hive::app::operation_get_impacted_accounts(op, accounts_new);
    }

    // Operations creating accounts that should not collect accounts to be created:

    // Ignore delegate_rc_operation as it is not included into the hive::protocol::operation type

    result_type operator()( const hive::protocol::custom_binary_operation& op )const
    {
      for(const auto& acc : op.required_owner_auths)
        accounts_new.insert( acc );
      for(const auto& acc : op.required_active_auths)
        accounts_new.insert( acc );
      for(const auto& acc : op.required_posting_auths)
        accounts_new.insert( acc );
      for(const auto& auth : op.required_auths)
        for(const auto& acc : auth.account_auths)
          accounts_new.insert( acc.first );
    }
    result_type operator()( const hive::protocol::custom_json_operation& op )const
    {
      for(const auto& acc : op.required_auths)
        accounts_new.insert( acc );
      for(const auto& acc : op.required_posting_auths)
        accounts_new.insert( acc );
    }
    result_type operator()( const hive::protocol::custom_operation& op )const
    {
      for(const auto& acc : op.required_auths)
        accounts_new.insert( acc );
    }
    result_type operator()( const hive::protocol::witness_update_operation& op )const
    {
      accounts_new.insert( op.owner );
    }
    result_type operator()( const hive::protocol::convert_operation& op )const
    {
      accounts_new.insert( op.owner );
    }
    result_type operator()( const hive::protocol::collateralized_convert_operation& op )const
    {
      accounts_new.insert( op.owner );
    }
    result_type operator()( const hive::protocol::limit_order_create_operation& op )const
    {
      accounts_new.insert( op.owner );
    }
    result_type operator()( const hive::protocol::limit_order_create2_operation& op )const
    {
      accounts_new.insert( op.owner );
    }
    result_type operator()( const hive::protocol::limit_order_cancel_operation& op )const
    {
      accounts_new.insert( op.owner );
    }
    result_type operator()( const hive::protocol::request_account_recovery_operation& op )const
    {
      accounts_new.insert( op.recovery_account );
      accounts_new.insert( op.account_to_recover );
      for(const auto& acc : op.new_owner_authority.account_auths)
        accounts_new.insert( acc.first );
    }
    result_type operator()( const hive::protocol::recover_account_operation& op )const
    {
      accounts_new.insert( op.account_to_recover );
      for(const auto& acc : op.new_owner_authority.account_auths)
        accounts_new.insert( acc.first );
      for(const auto& acc : op.recent_owner_authority.account_auths)
        accounts_new.insert( acc.first );
    }
    result_type operator()( const hive::protocol::reset_account_operation& op )const
    {
      accounts_new.insert( op.account_to_reset );
      accounts_new.insert( op.reset_account );
      for(const auto& acc : op.new_owner_authority.account_auths)
        accounts_new.insert( acc.first );
    }
    result_type operator()( const hive::protocol::set_reset_account_operation& op )const
    {
      accounts_new.insert( op.account );
      accounts_new.insert( op.current_reset_account );
      accounts_new.insert( op.reset_account );
    }
    result_type operator()( const hive::protocol::change_recovery_account_operation& op )const
    {
      accounts_new.insert( op.account_to_recover );
      accounts_new.insert( op.new_recovery_account );
    }
    result_type operator()( const hive::protocol::cancel_transfer_from_savings_operation& op )const
    {
      accounts_new.insert( op.from );
    }
    result_type operator()( const hive::protocol::decline_voting_rights_operation& op )const
    {
      accounts_new.insert( op.account );
    }
    result_type operator()( const hive::protocol::claim_reward_balance_operation& op )const
    {
      accounts_new.insert( op.account );
    }
    result_type operator()( const hive::protocol::witness_block_approve_operation& op )const
    {
      accounts_new.insert( op.witness );
    }
    result_type operator()( const hive::protocol::withdraw_vesting_operation& op )const
    {
      accounts_new.insert( op.account );
    }
    result_type operator()( const hive::protocol::account_update2_operation& op )const
    {
      accounts_new.insert( op.account );
      if(op.owner.valid())
        for(const auto& acc : op.owner->account_auths)
          accounts_new.insert( acc.first );
      if(op.active.valid())
        for(const auto& acc : op.active->account_auths)
          accounts_new.insert( acc.first );
      if(op.posting.valid())
        for(const auto& acc : op.posting->account_auths)
          accounts_new.insert( acc.first );
    }
    result_type operator()( const hive::protocol::account_update_operation& op )const
    {
      accounts_new.insert( op.account );
      if(op.owner.valid())
        for(const auto& acc : op.owner->account_auths)
          accounts_new.insert( acc.first );
      if(op.active.valid())
        for(const auto& acc : op.active->account_auths)
          accounts_new.insert( acc.first );
      if(op.posting.valid())
        for(const auto& acc : op.posting->account_auths)
          accounts_new.insert( acc.first );
    }
    result_type operator()( const hive::protocol::claim_account_operation& op )const
    {
      accounts_new.insert( op.creator );
    }
    result_type operator()( const hive::protocol::create_claimed_account_operation& op )const
    {
      accounts_new.insert( op.creator );
      accounts_created.insert( op.new_account_name );
      for(const auto& acc : op.owner.account_auths)
        accounts_new.insert( acc.first );
      for(const auto& acc : op.active.account_auths)
        accounts_new.insert( acc.first );
      for(const auto& acc : op.posting.account_auths)
        accounts_new.insert( acc.first );
    }
    result_type operator()( const hive::protocol::account_create_operation& op )const
    {
      accounts_new.insert( op.creator );
      accounts_created.insert( op.new_account_name );
      for(const auto& acc : op.owner.account_auths)
        accounts_new.insert( acc.first );
      for(const auto& acc : op.active.account_auths)
        accounts_new.insert( acc.first );
      for(const auto& acc : op.posting.account_auths)
        accounts_new.insert( acc.first );
    }
    result_type operator()( const hive::protocol::pow_operation& op )const
    {
      accounts_created.insert( op.worker_account );
    }
    result_type operator()( const hive::protocol::account_create_with_delegation_operation& op )const
    {
      accounts_new.insert( op.creator );
      accounts_created.insert( op.new_account_name );
      for(const auto& acc : op.owner.account_auths)
        accounts_new.insert( acc.first );
      for(const auto& acc : op.active.account_auths)
        accounts_new.insert( acc.first );
      for(const auto& acc : op.posting.account_auths)
        accounts_new.insert( acc.first );
    }
    result_type operator()( hive::protocol::pow2_operation& op )const {
      const auto& input = op.work.which() ? op.work.get< hive::protocol::equihash_pow >().input : op.work.get< hive::protocol::pow2 >().input;

      if( accounts_created.find( input.worker_account ) == accounts_created.end() )
      {
        if( !op.new_owner_key.valid() )
          op.new_owner_key = converter.get_witness_key().get_public_key();

        accounts_created.insert( input.worker_account );
      }
    }
  };

} } } } }

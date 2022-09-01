#include <hive/protocol/authority.hpp>

#include <hive/protocol/forward_impacted.hpp>

#include <fc/utility.hpp>

namespace hive { namespace app {

using namespace fc;
using namespace hive::protocol;

// TODO:  Review all of these, especially no-ops
struct get_impacted_account_visitor
{
  flat_set<account_name_type>& _impacted;
  get_impacted_account_visitor( flat_set<account_name_type>& impact ):_impacted( impact ) {}
  typedef void result_type;

  template<typename T>
  void operator()( const T& op )
  {
    op.get_required_posting_authorities( _impacted );
    op.get_required_active_authorities( _impacted );
    op.get_required_owner_authorities( _impacted );
  }

  // ops
  void operator()( const account_create_operation& op )
  {
    _impacted.insert( op.new_account_name );
    _impacted.insert( op.creator );
  }

  void operator()( const account_create_with_delegation_operation& op )
  {
    _impacted.insert( op.new_account_name );
    _impacted.insert( op.creator );
  }

  void operator()( const account_created_operation& op )
  {
    _impacted.insert( op.creator );
    _impacted.insert( op.new_account_name );
  }

  void operator()( const comment_operation& op )
  {
    _impacted.insert( op.author );
    if( op.parent_author.size() )
      _impacted.insert( op.parent_author );
  }

  void operator()( const vote_operation& op )
  {
    _impacted.insert( op.voter );
    _impacted.insert( op.author );
  }

  void operator()( const transfer_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
  }

  void operator()( const escrow_transfer_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
    _impacted.insert( op.agent );
  }

  void operator()( const escrow_approve_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
    _impacted.insert( op.agent );
  }

  void operator()( const escrow_dispute_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
    _impacted.insert( op.agent );
  }

  void operator()( const escrow_release_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
    _impacted.insert( op.agent );
  }

  void operator()( const transfer_to_vesting_operation& op )
  {
    _impacted.insert( op.from );

    if ( op.to != account_name_type() && op.to != op.from )
    {
      _impacted.insert( op.to );
    }
  }

  void operator()( const set_withdraw_vesting_route_operation& op )
  {
    _impacted.insert( op.from_account );
    _impacted.insert( op.to_account );
  }

  void operator()( const account_witness_vote_operation& op )
  {
    _impacted.insert( op.account );
    _impacted.insert( op.witness );
  }

  void operator()( const account_witness_proxy_operation& op )
  {
    _impacted.insert( op.account );
    if ( !op.is_clearing_proxy() )
      _impacted.insert( op.proxy );
  }

  void operator()( const feed_publish_operation& op )
  {
    _impacted.insert( op.publisher );
  }

  void operator()( const pow_operation& op )
  {
    _impacted.insert( op.worker_account );
  }

  struct pow2_impacted_visitor
  {
    pow2_impacted_visitor(){}

    typedef const account_name_type& result_type;

    template< typename WorkType >
    result_type operator()( const WorkType& work )const
    {
      return work.input.worker_account;
    }
  };

  void operator()( const pow2_operation& op )
  {
    _impacted.insert( op.work.visit( pow2_impacted_visitor() ) );
  }

  void operator()( const request_account_recovery_operation& op )
  {
    _impacted.insert( op.account_to_recover );
    _impacted.insert( op.recovery_account );
  }

  void operator()( const recover_account_operation& op )
  {
    _impacted.insert( op.account_to_recover );
  }

  void operator()( const change_recovery_account_operation& op )
  {
    _impacted.insert( op.account_to_recover );
  }

  void operator()( const transfer_to_savings_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
  }

  void operator()( const transfer_from_savings_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
  }

  void operator()( const delegate_vesting_shares_operation& op )
  {
    _impacted.insert( op.delegator );
    _impacted.insert( op.delegatee );
  }

  void operator()( const witness_set_properties_operation& op )
  {
    _impacted.insert( op.owner );
  }

  void operator()( const create_claimed_account_operation& op )
  {
    _impacted.insert( op.creator );
    _impacted.insert( op.new_account_name );
  }

  void operator()( const recurrent_transfer_operation& op )
  {
      _impacted.insert( op.from );
      _impacted.insert( op.to );
  }

  // vops

  void operator()( const author_reward_operation& op )
  {
    _impacted.insert( op.author );
  }

  void operator()( const curation_reward_operation& op )
  {
    _impacted.insert( op.curator );
  }

  void operator()( const liquidity_reward_operation& op )
  {
    _impacted.insert( op.owner );
  }

  void operator()( const interest_operation& op )
  {
    _impacted.insert( op.owner );
  }

  void operator()( const fill_convert_request_operation& op )
  {
    _impacted.insert( op.owner );
  }

  void operator()( const fill_collateralized_convert_request_operation& op )
  {
    _impacted.insert( op.owner );
  }

  void operator()( const fill_vesting_withdraw_operation& op )
  {
    _impacted.insert( op.from_account );
    _impacted.insert( op.to_account );
  }

  void operator()( const transfer_to_vesting_completed_operation& op )
  {
    _impacted.insert( op.from_account );
    _impacted.insert( op.to_account );
  }

  void operator()( const pow_reward_operation& op )
  {
    _impacted.insert( op.worker );
  }

  void operator()( const vesting_shares_split_operation& op )
  {
    _impacted.insert( op.owner );
  }

  void operator()( const shutdown_witness_operation& op )
  {
    _impacted.insert( op.owner );
  }

  void operator()( const fill_order_operation& op )
  {
    _impacted.insert( op.current_owner );
    _impacted.insert( op.open_owner );
  }

  void operator()( const fill_transfer_from_savings_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
  }

  void operator()( const return_vesting_delegation_operation& op )
  {
    _impacted.insert( op.account );
  }

  void operator()(const comment_reward_operation& op)
  {
    _impacted.insert(op.author);
  }

  void operator()(const effective_comment_vote_operation& op)
  {
    _impacted.emplace(op.author);
    _impacted.emplace(op.voter);
  }

  void operator()(const ineffective_delete_comment_operation& op)
  {
    _impacted.emplace(op.author);
  }

  void operator()(const comment_payout_update_operation& op)
  {
    _impacted.insert(op.author);
  }

  void operator()( const comment_benefactor_reward_operation& op )
  {
    _impacted.insert( op.benefactor );
    _impacted.insert( op.author );
  }

  void operator()( const producer_reward_operation& op )
  {
    _impacted.insert( op.producer );
  }

  void operator()(const proposal_pay_operation& op)
  {
    _impacted.insert(op.receiver);
    _impacted.insert(op.payer);
  }

  void operator()( const create_proposal_operation& op )
  {
    _impacted.insert( op.creator );
    _impacted.insert( op.receiver );
  }

  void operator()( const update_proposal_operation& op )
  {
    _impacted.insert( op.creator );
  }

  void operator()( const update_proposal_votes_operation& op )
  {
    _impacted.insert( op.voter );
  }

  void operator()( const remove_proposal_operation& op )
  {
    _impacted.insert( op.proposal_owner );
  }

  void operator()( const sps_fund_operation& op )
  {
    _impacted.insert( op.fund_account );
  }

  void operator()( const delayed_voting_operation& op )
  {
    _impacted.insert( op.voter );
  }

  void operator()( const hardfork_operation& op )
  {
    _impacted.insert( HIVE_INIT_MINER_NAME );
  }

  void operator()( const hardfork_hive_operation& op )
  {
    _impacted.insert( op.treasury );
    _impacted.insert( op.account );
  }

  void operator()( const hardfork_hive_restore_operation& op )
  {
    _impacted.insert( op.treasury );
    _impacted.insert( op.account );
  }

  void operator()( const sps_convert_operation& op )
  {
    _impacted.insert( op.fund_account );
  }

  void operator()( const consolidate_treasury_balance_operation& op )
  {
    _impacted.insert( NEW_HIVE_TREASURY_ACCOUNT );
    _impacted.insert( OBSOLETE_TREASURY_ACCOUNT );
  }

  void operator()( const clear_null_account_balance_operation& op )
  {
    _impacted.insert( HIVE_NULL_ACCOUNT );
  }

  void operator()( const expired_account_notification_operation& op )
  {
    _impacted.insert( op.account );
  }

  void operator()( const changed_recovery_account_operation& op )
  {
    _impacted.insert( op.account );
    _impacted.insert( op.old_recovery_account );
    _impacted.insert( op.new_recovery_account );
  }

  void operator()( const system_warning_operation& op )
  {
    _impacted.insert( HIVE_INIT_MINER_NAME );
  }


  void operator()( const fill_recurrent_transfer_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
  }

  void operator()( const failed_recurrent_transfer_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
  }

  //void operator()( const operation& op ){}
};

void operation_get_impacted_accounts( const operation& op, flat_set<account_name_type>& result )
{
  get_impacted_account_visitor vtor = get_impacted_account_visitor( result );
  op.visit( vtor );
}

namespace /// anonymous
{

struct impacted_balance_collector
{
  impacted_balance_data result;

  typedef void result_type;

  template <class T>
  void operator()(const T&) 
  {
    if(result.empty())
    {
      result.emplace_back(account_name_type("blocktrades"), asset(12345, HIVE_SYMBOL));
      result.emplace_back(account_name_type("sender2"), asset(-12345, HIVE_SYMBOL));

      result.emplace_back(account_name_type("blocktrades"), asset(54321, HBD_SYMBOL));
      result.emplace_back(account_name_type("receiver"), asset(-54321, HBD_SYMBOL));

      result.emplace_back(account_name_type("blocktrades"), asset(54321, VESTS_SYMBOL));
      result.emplace_back(account_name_type("receiver"), asset(-54321, VESTS_SYMBOL));
    }
  }
};

} /// anonymous

impacted_balance_data operation_get_impacted_balances(const hive::protocol::operation& op)
{
  impacted_balance_collector collector;

  op.visit(collector);
  
  return std::move(collector.result);
}

void transaction_get_impacted_accounts( const transaction& tx, flat_set<account_name_type>& result )
{
  for( const auto& op : tx.operations )
    operation_get_impacted_accounts( op, result );
}

} }

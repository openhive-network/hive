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

  void operator()(const limit_order_cancelled_operation& op)
  {
    _impacted.insert(op.seller);
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

  void operator()( const dhf_funding_operation& op )
  {
    _impacted.insert( op.treasury );
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
    _impacted.insert( op.other_affected_accounts.begin(), op.other_affected_accounts.end() );
  }

  void operator()( const hardfork_hive_restore_operation& op )
  {
    _impacted.insert( op.treasury );
    _impacted.insert( op.account );
  }

  void operator()( const dhf_conversion_operation& op )
  {
    _impacted.insert( op.treasury );
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

  void operator()( const producer_missed_operation& op )
  {
    _impacted.insert( op.producer );
  }

  void operator()( const proposal_fee_operation& op )
  {
    _impacted.insert( op.creator );
    _impacted.insert( op.treasury );
  }

  void operator()( const collateralized_convert_immediate_conversion_operation& op )
  {
    _impacted.insert( op.owner );
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

/**
 * @brief Visitor collects changes to account balances to be involved by given operation.
*/
struct impacted_balance_collector
{
  private:

    const bool is_hardfork_1        = false;
    const uint32_t VESTS_SCALING_FACTOR  = 1'000'000;

    void emplace_back(const protocol::account_name_type& account, const asset& a)
    {
      /*
        There was in the block 905693 a HF1 that generated bunch of virtual operations `vesting_shares_split_operation`( above 5000 ).
        This operation multiplied VESTS by milion for every account.
      */
      if( !is_hardfork_1 && is_asset_type( a, VESTS_SYMBOL ) )
      {
        result.emplace_back(account, asset(a.amount * VESTS_SCALING_FACTOR, a.symbol));
      }
      else
        result.emplace_back(account, a);
    }

  public:

  impacted_balance_collector(const bool is_hardfork_1): is_hardfork_1( is_hardfork_1 ){}

  impacted_balance_data result;

  typedef void result_type;

  void operator()(const fill_vesting_withdraw_operation& o)
  {
    emplace_back(o.to_account, o.deposited);
    emplace_back(o.from_account, -o.withdrawn);
  }

  void operator()(const producer_reward_operation& o)
  {
    emplace_back(o.producer, o.vesting_shares);
  }

  void operator()(const claim_account_operation& o)
  {
    emplace_back(o.creator, -o.fee);
    emplace_back(account_name_type(HIVE_NULL_ACCOUNT), o.fee);
  }

  void operator()(const account_create_operation& o)
  {
    emplace_back(o.creator, -o.fee);
    emplace_back(account_name_type(HIVE_NULL_ACCOUNT), o.fee);
  }

  void operator()(const account_create_with_delegation_operation& o)
  {
    emplace_back(o.creator, -o.fee);
    emplace_back(account_name_type(HIVE_NULL_ACCOUNT), o.fee);
  }

  void operator()(const hardfork_hive_restore_operation& o)
  {
    if(o.hbd_transferred.amount != 0)
    {
      emplace_back(o.account, o.hbd_transferred);
      emplace_back(o.treasury, -o.hbd_transferred);
    }
    if(o.hive_transferred.amount != 0)
    {
      emplace_back(o.account, o.hive_transferred);
      emplace_back(o.treasury, -o.hive_transferred);
    }
  }

  void operator()(const fill_recurrent_transfer_operation& o)
  {
    emplace_back(o.to, o.amount);
    emplace_back(o.from, -o.amount);
  }

  void operator()(const fill_transfer_from_savings_operation& o)
  {
    emplace_back(o.to, o.amount);
  }

  void operator()(const liquidity_reward_operation& o)
  {
    emplace_back(o.owner, o.payout);
  }

  void operator()(const fill_convert_request_operation& o)
  {
    emplace_back(o.owner, o.amount_out);
  }

  void operator()(const fill_collateralized_convert_request_operation& o)
  {
    emplace_back(o.owner, o.excess_collateral);
  }

  void operator()(const escrow_transfer_operation& o)
  {
    asset hive_spent = o.hive_amount;
    asset hbd_spent = o.hbd_amount;
    if(o.fee.symbol == HIVE_SYMBOL)
      hive_spent += o.fee;
    else
      hbd_spent += o.fee;

    if(hive_spent.amount != 0)
      emplace_back(o.from, -hive_spent);
  
    if(hbd_spent.amount != 0)
      emplace_back(o.from, -hbd_spent);
  }

  void operator()(const escrow_release_operation& o)
  {
    if(o.hive_amount.amount != 0)
      emplace_back(o.receiver, o.hive_amount);

    if(o.hbd_amount.amount != 0)
      emplace_back(o.receiver, o.hbd_amount);
  }

  void operator()(const transfer_operation& o)
  {
    emplace_back(o.from, -o.amount);
    emplace_back(o.to, o.amount);
  }

  void operator()(const transfer_to_vesting_operation& o)
  {
    /// Nothing to do in favor to transfer_to_vesting_completed_operation
  }

  void operator()(const transfer_to_vesting_completed_operation& o)
  {
    emplace_back(o.to_account, o.vesting_shares_received);
    emplace_back(o.from_account, -o.hive_vested);
  }

  void operator()(const pow_reward_operation& o)
  {
    emplace_back(o.worker, o.reward);
  }

  void operator()(const limit_order_create_operation& o)
  {
    emplace_back(o.owner, -o.amount_to_sell);
  }

  void operator()(const limit_order_create2_operation& o)
  {
    emplace_back(o.owner, -o.amount_to_sell);
  }

  void operator()(const transfer_to_savings_operation& o)
  {
    emplace_back(o.from, -o.amount);
  }

  void operator()(const fill_order_operation& o)
  {
    emplace_back(o.open_owner, o.current_pays);
    emplace_back(o.current_owner, o.open_pays);
  }

  void operator()(const limit_order_cancelled_operation& o)
  {
    emplace_back(o.seller, o.amount_back);
  }
  
  void operator()(const claim_reward_balance_operation& o)
  {
    if(o.reward_hive.amount != 0)
      emplace_back(o.account, o.reward_hive);
    if(o.reward_hbd.amount != 0)
      emplace_back(o.account, o.reward_hbd);
    if(o.reward_vests.amount != 0)
      emplace_back(o.account, o.reward_vests);
  }

  void operator()(const proposal_pay_operation& o)
  {
    emplace_back(o.receiver, o.payment);
    emplace_back(o.payer, -o.payment);
  }

  void operator()(const dhf_conversion_operation& o)
  {
    emplace_back(o.treasury, -o.hive_amount_in);
    emplace_back(o.treasury, o.hbd_amount_out);
  }

  void operator()(const author_reward_operation& o)
  {
    if( !o.payout_must_be_claimed )
    {
      emplace_back(o.author, o.hbd_payout);
      emplace_back(o.author, o.hive_payout);
      emplace_back(o.author, o.vesting_payout);
    }
  }

  void operator()(const comment_benefactor_reward_operation& o)
  {
    if( !o.payout_must_be_claimed )
    {
      emplace_back(o.benefactor, o.hbd_payout);
      emplace_back(o.benefactor, o.hive_payout);
      emplace_back(o.benefactor, o.vesting_payout);
    }
  }

  void operator()(const curation_reward_operation& o)
  {
    if( !o.payout_must_be_claimed )
    {
      emplace_back(o.curator, o.reward);
    }
  }

  void operator()( const account_created_operation& op )
  {
    emplace_back(op.new_account_name, op.initial_vesting_shares);
  }

  void operator()( const interest_operation& op )
  {
    if(op.is_saved_into_hbd_balance)
      emplace_back(op.owner, op.interest);
  }

  void operator()( const proposal_fee_operation& op )
  {
    emplace_back(op.creator, -op.fee);
    emplace_back(op.treasury, op.fee);
  }

  void operator()( const collateralized_convert_immediate_conversion_operation& op )
  {
    emplace_back(op.owner, op.hbd_out);
  }

  template <class T>
  void operator()(const T&) 
  {
    /// Nothing to do for non-financial ops.
  }
};

} /// anonymous

impacted_balance_data operation_get_impacted_balances(const hive::protocol::operation& op, const bool is_hardfork_1)
{
  impacted_balance_collector collector(is_hardfork_1);

  op.visit(collector);
  
  return std::move(collector.result);
}

void transaction_get_impacted_accounts( const transaction& tx, flat_set<account_name_type>& result )
{
  for( const auto& op : tx.operations )
    operation_get_impacted_accounts( op, result );
}

} }

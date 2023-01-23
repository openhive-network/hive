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
    if( !op.is_empty_implied_route() )
    {
      _impacted.insert( op.from_account );
      _impacted.insert( op.to_account );
    }
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

  void operator()( const escrow_approved_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
    _impacted.insert( op.agent );
  }

  void operator()( const escrow_rejected_operation& op )
  {
    _impacted.insert( op.from );
    _impacted.insert( op.to );
    _impacted.insert( op.agent );
  }

  void operator()( const proxy_cleared_operation& op )
  {
    _impacted.insert( op.account );
    _impacted.insert( op.proxy );
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
  struct get_created_from_account_create_operations_visitor
  {
    get_created_from_account_create_operations_visitor(){}

    typedef account_name_type result_type;

    result_type operator()( const account_create_operation& op )const
    {
      return op.new_account_name;
    }

    result_type operator()( const account_create_with_delegation_operation& op )const
    {
      return op.new_account_name;
    }

    result_type operator()( const create_claimed_account_operation& op )const
    {
      return op.new_account_name;
    }

    result_type operator()( const account_created_operation& op )const
    {
      return op.new_account_name;
    }

    template< typename T >
    result_type operator()( [[maybe_unused]] const T& op )const
    {
      FC_ASSERT( false, "Visitor meant to be used only with the account_created_operation" );
    }
  };
}

account_name_type get_created_from_account_create_operations( const operation& op )
{
  return op.visit( get_created_from_account_create_operations_visitor{} );
}

namespace /// anonymous
{

struct get_static_variant_name_with_prefix
{
  string& name;
  get_static_variant_name_with_prefix( string& n )
      : name( n ) {}

  typedef void result_type;

  template< typename T > void operator()( const T& v )const
  {
      name = fc::get_typename< T >::name();
  }
};


template <typename T>
void exclude_from_used_operations(hive::app::stringset& used_operations)
{
  used_operations.erase(fc::get_typename<T>::name());
}

template<typename Collector>
hive::app::stringset run_all_visitor_overloads(Collector& k)
{
    //all type strings and variant instances into a map
    std::map< string, hive::protocol::operation > string_variant_map;

    for( int i = 0; i < hive::protocol::operation::count(); ++i )
    {
        hive::protocol::operation variant;
        variant.set_which(i);
        string operation_name;
        variant.visit( get_static_variant_name_with_prefix( operation_name ) );
        string_variant_map[operation_name] = variant;
    }
    
    k.used_operations.clear();
    //collect all type strings
    for(const auto& [s, _]: string_variant_map)
    {
      k.used_operations.insert(s);
    }
    
    //call all overloads by visiting - inside overload remove unused type strings with exclude_from_used_operations
    for(const auto& [_, variant_instance]: string_variant_map)
    {
      variant_instance.visit(k);
    }

    return k.used_operations;
}


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
      if( a.amount == 0 )
        return;
      /*
        There was in the block 905693 a HF1 that generated bunch of virtual operations `vesting_shares_split_operation`( above 5000 ).
        This operation multiplied VESTS by milion for every account.
      */
      if( !is_hardfork_1 && is_asset_type( a, VESTS_SYMBOL ) )
        result.emplace_back(account, asset(a.amount * VESTS_SCALING_FACTOR, a.symbol));
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

  void operator()(const clear_null_account_balance_operation& o)
  {
    // balance tracker and similar apps need to clear all balances or even skip handling null account altogether
    // vop will not contain all the information necessary (f.e. it only has summary including savings and pending
    // rewards, adding all details would bloat the vop)
  }

  void operator()(const consolidate_treasury_balance_operation& o)
  {
    // balance tracker and similar apps need to clear all balances of OBSOLETE_TREASURY_ACCOUNT like in case
    // of clear_null_account_balance_operation
    // NOTE: there is a potential problem, since the related routine rewrites balances one to one; normally
    // treasury cannot have rewards or savings, but if it somehow did (f.e. if old treasury were treated like normal
    // account due to bug), then we'd be erroneously counting reward/saving balances towards liquid balances
    for( auto& value : o.total_moved )
      emplace_back(NEW_HIVE_TREASURY_ACCOUNT, value);
  }

  void operator()(const claim_account_operation& o)
  {
    emplace_back(o.creator, -o.fee);
    emplace_back(account_name_type(HIVE_NULL_ACCOUNT), o.fee);
  }

  void operator()(const account_create_operation& o)
  {
    emplace_back(o.creator, -o.fee);
    // fee only goes to null after HF20 (before that it becomes initial vests), but since
    // transfer to null means burning, maybe we can live with that short lived difference in balance
    emplace_back(account_name_type(HIVE_NULL_ACCOUNT), o.fee);
  }

  void operator()(const account_create_with_delegation_operation& o)
  {
    emplace_back(o.creator, -o.fee);
    // fee only goes to null after HF20 (before that it becomes initial vests), but since
    // transfer to null means burning, maybe we can live with that short lived difference in balance
    emplace_back(account_name_type(HIVE_NULL_ACCOUNT), o.fee);
  }

  void operator()(const hardfork_hive_operation& o)
  {
    // balance tracker and similar apps need to clear all balances for accounts pointed by o.account
    emplace_back(o.treasury, o.hive_transferred);
    emplace_back(o.treasury, o.hbd_transferred);
    emplace_back(o.treasury, o.total_hive_from_vests);
  }

  void operator()(const hardfork_hive_restore_operation& o)
  {
    emplace_back(o.account, o.hbd_transferred);
    emplace_back(o.treasury, -o.hbd_transferred);
    emplace_back(o.account, o.hive_transferred);
    emplace_back(o.treasury, -o.hive_transferred);
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

    emplace_back(o.from, -hive_spent);
    emplace_back(o.from, -hbd_spent);
  }

  void operator()(const escrow_release_operation& o)
  {
    emplace_back(o.receiver, o.hive_amount);
    emplace_back(o.receiver, o.hbd_amount);
  }

  void operator()(const escrow_approve_operation& o)
  {
    // Nothing to do in favor of escrow_approved_operation/escrow_rejected_operation
  }

  void operator()(const transfer_operation& o)
  {
    emplace_back(o.from, -o.amount);
    emplace_back(o.to, o.amount);
  }

  void operator()(const transfer_to_vesting_operation& o)
  {
    // Nothing to do in favor of transfer_to_vesting_completed_operation
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

  void operator()(const limit_order_cancel_operation& o)
  {
    // Nothing to do in favor of limit_order_cancelled_operation
  }

  void operator()(const limit_order_create2_operation& o)
  {
    emplace_back(o.owner, -o.amount_to_sell);
  }

  void operator()(const convert_operation& o)
  {
    emplace_back(o.owner, -o.amount);
  }

  void operator()(const collateralized_convert_operation& o)
  {
    emplace_back(o.owner, -o.amount);
    // Note: HBD received handled in collateralized_convert_immediate_conversion_operation
  }

  void operator()(const transfer_to_savings_operation& o)
  {
    emplace_back(o.from, -o.amount);
  }

  void operator()( const transfer_from_savings_operation& o)
  {
    // Nothing to do in favor of fill_transfer_from_savings_operation
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
    emplace_back(o.account, o.reward_hive);
    emplace_back(o.account, o.reward_hbd);
    emplace_back(o.account, o.reward_vests);
  }

  void operator()(const proposal_pay_operation& o)
  {
    emplace_back(o.receiver, o.payment);
    emplace_back(o.payer, -o.payment);
  }

  void operator()(const dhf_funding_operation& o)
  {
    emplace_back(o.treasury, o.additional_funds);
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

  void operator()( const escrow_approved_operation& op )
  {
    emplace_back( op.agent, op.fee );
  }

  void operator()( const escrow_rejected_operation& op )
  {
    emplace_back( op.from, op.hbd_amount );
    emplace_back( op.from, op.hive_amount );
    emplace_back( op.from, op.fee );
  }

  template <typename T>
  void operator()(const T& op) 
  {
    exclude_from_used_operations<T>(used_operations);
  }

  hive::app::stringset used_operations;  
};


struct keyauth_collector
{
  collected_keyauth_collection_t collected_keyauths;

  typedef void result_type;

private:
  
  auto get_keys(const authority& _authority)
  {
    return _authority.get_keys();
  }

  auto get_keys(const optional<authority>& _authority)
  {
    if(_authority)
      return _authority->get_keys();
    return std::vector<public_key_type>();
  }

  template<typename T, typename AT> 
  void collect_one(
    const T& _op,
    const AT& _authority,
    authority_t _authority_kind,
    std::string _account_name)
  {
    collected_keyauth_t collected_item;
    collected_item.account_name   = _account_name;
    collected_item.authority_kind = _authority_kind;

    for(const auto& key_data: get_keys(_authority))
    {
      std::string s = static_cast<std::string>(key_data);
      collected_item.key_auth = s;
      collected_keyauths.emplace_back(collected_item);
    }
  }

  public:


  // ops
  result_type operator()( const account_create_operation& op )
  {
    collect_one(op, op.owner,   hive::app::authority_t::OWNER,   op.new_account_name);
    collect_one(op, op.active,  hive::app::authority_t::ACTIVE,  op.new_account_name);
    collect_one(op, op.posting, hive::app::authority_t::POSTING, op.new_account_name);
  }

  result_type operator()( const account_create_with_delegation_operation& op )
  {
    collect_one(op, op.owner,   hive::app::authority_t::OWNER,   op.new_account_name);
    collect_one(op, op.active,  hive::app::authority_t::ACTIVE,  op.new_account_name);
    collect_one(op, op.posting, hive::app::authority_t::POSTING, op.new_account_name);
  }
  
  result_type operator()( const account_update_operation& op )
  {
    collect_one(op, op.owner,  hive::app::authority_t::OWNER,  op.account);
    collect_one(op, op.active, hive::app::authority_t::ACTIVE, op.account);
    collect_one(op, op.posting,hive::app::authority_t::POSTING, op.account);
  }
  
  result_type operator()( const account_update2_operation& op )
  {
    collect_one(op, op.owner,  hive::app::authority_t::OWNER,  op.account);
    collect_one(op, op.active, hive::app::authority_t::ACTIVE, op.account);
    collect_one(op, op.posting,hive::app::authority_t::POSTING, op.account);
  }
  
  result_type operator()( const create_claimed_account_operation& op )
  {
    collect_one(op, op.owner,  hive::app::authority_t::OWNER,  op.new_account_name);
    collect_one(op, op.active, hive::app::authority_t::ACTIVE, op.new_account_name);
    collect_one(op, op.posting,hive::app::authority_t::POSTING, op.new_account_name);
  }

  result_type operator()( const recover_account_operation& op)
  {
    collect_one(op, op.new_owner_authority,    hive::app::authority_t::NEW_OWNER_AUTHORITY,    op.account_to_recover);
    collect_one(op, op.recent_owner_authority, hive::app::authority_t::RECENT_OWNER_AUTHORITY, op.account_to_recover);
  }

  result_type operator()( const reset_account_operation& op) 
  {
    collect_one(op, op.new_owner_authority, hive::app::authority_t::NEW_OWNER_AUTHORITY, op.account_to_reset);
  }
  
  result_type operator()( const request_account_recovery_operation& op)
  {
    collect_one(op, op.new_owner_authority, hive::app::authority_t::NEW_OWNER_AUTHORITY, op.account_to_recover);
    collect_one(op, op.new_owner_authority, hive::app::authority_t::NEW_OWNER_AUTHORITY, op.recovery_account);
  }

  result_type operator()( const witness_set_properties_operation& op )
  {
    vector< authority > authorities;
    op.get_required_authorities(authorities);
    for(const auto& authority : authorities)
    {
      collect_one(op, authority, hive::app::authority_t::WITNESS,  op.owner);
    }
  }

  template <typename T>
  void operator()(const T& op) 
  {
    exclude_from_used_operations<T>(used_operations);
  }

  hive::app::stringset used_operations;
};



} /// anonymous

impacted_balance_data operation_get_impacted_balances(const hive::protocol::operation& op, const bool is_hardfork_1)
{
  impacted_balance_collector collector(is_hardfork_1);

  op.visit(collector);
  
  return std::move(collector.result);
}

stringset get_operations_used_in_get_balance_impacting_operations()
{
  impacted_balance_collector collector(true);
  static auto used_operations = run_all_visitor_overloads(collector);
  return used_operations;
}


collected_keyauth_collection_t operation_get_keyauths(const hive::protocol::operation& op)
{
  keyauth_collector collector;

  op.visit(collector);
  
  return std::move(collector.collected_keyauths);
}

stringset get_operations_used_in_get_keyauths()
{
  keyauth_collector collector;
  static auto used_operations = run_all_visitor_overloads(collector);
  return used_operations;
}

void transaction_get_impacted_accounts( const transaction& tx, flat_set<account_name_type>& result )
{
  for( const auto& op : tx.operations )
    operation_get_impacted_accounts( op, result );
}

} }

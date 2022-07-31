#pragma once
#include <hive/protocol/base.hpp>
#include <hive/protocol/block_header.hpp>
#include <hive/protocol/asset.hpp>

#include <fc/exception/exception.hpp>
#include <fc/utf8.hpp>

namespace hive { namespace protocol {

/**
  * Related to convert_operation.
  * Generated during block processing after conversion delay passes and HBD is converted to HIVE.
  */
struct fill_convert_request_operation : public virtual_operation
{
  fill_convert_request_operation() = default;
  fill_convert_request_operation( const account_name_type& o, const uint32_t id, const HBD_asset& in, const HIVE_asset& out )
    : owner( o ), requestid( id ), amount_in( in ), amount_out( out )
  {}

  account_name_type owner; //user that requested conversion (receiver of amount_out)
  uint32_t          requestid = 0; //id of the request
  asset             amount_in; //(HBD) source of conversion
  asset             amount_out; //(HIVE) effect of conversion
};

/**
  * Related to comment_operation.
  * Generated during block processing after cashout time passes and comment is eligible for rewards (nonzero reward).
  * Note: the reward is the author portion of comment reward lowered by the rewards distributed towards beneficiaries
  * (therefore it can be zero).
  * @see comment_benefactor_reward_operation
  */
struct author_reward_operation : public virtual_operation
{
  author_reward_operation() = default;
  author_reward_operation( const account_name_type& a, const string& p, const asset& s, const asset& st, const asset& v, const asset& c, bool must_be_claimed )
    : author( a ), permlink( p ), hbd_payout( s ), hive_payout( st ), vesting_payout( v ), curators_vesting_payout( c ), payout_must_be_claimed( must_be_claimed )
  {}

  account_name_type author; //author of the comment (receiver of hbd_payout, hive_payout, vesting_payout)
  string            permlink; //permlink of the comment
  asset             hbd_payout; //(HBD) part of reward
  asset             hive_payout; //(HIVE) part of reward
  asset             vesting_payout; //(VESTS) part of reward
  asset             curators_vesting_payout; //(VESTS) curators' portion of comment reward (@see curation_reward_operation)
  bool              payout_must_be_claimed = false; //true if payouts require use of claim_reward_balance_operation
};

/**
  * Related to comment_operation and comment_vote_operation.
  * Generated during block processing after cashout time passes and comment is eligible for rewards (nonzero reward).
  * Note: the reward is a fragment of curators' portion of comment reward depending on share of particular curator in overall
  * curation power for the comment. Only generated when nonzero.
  */
struct curation_reward_operation : public virtual_operation
{
  curation_reward_operation() = default;
  curation_reward_operation( const account_name_type& c, const asset& r, const account_name_type& a, const string& p, bool must_be_claimed )
    : curator( c ), reward( r ), comment_author( a ), comment_permlink( p ), payout_must_be_claimed( must_be_claimed )
  {}

  account_name_type curator; //user that curated the comment (receiver of reward)
  asset             reward; //(VESTS) curation reward
  account_name_type comment_author; //author of curated comment
  string            comment_permlink; //permlink of curated comment
  bool              payout_must_be_claimed = false; //true if payouts require use of claim_reward_balance_operation
};

/**
  * Related to comment_operation.
  * Generated during block processing after cashout time passes and comment is eligible for rewards (nonzero reward).
  * Note: for informational purposes only, shows summary of comment reward, does not indicate any transfers.
  * @see curation_reward_operation
  * @see comment_benefactor_reward_operation
  * @see author_reward_operation
  */
struct comment_reward_operation : public virtual_operation
{
  comment_reward_operation() = default;
  comment_reward_operation( const account_name_type& a, const string& pl, const HBD_asset& p, share_type ar,
    const HBD_asset& tpv, const HBD_asset& cpv, const HBD_asset& bpv )
    : author( a ), permlink( pl ), payout( p ), author_rewards( ar ),
    total_payout_value( tpv ), curator_payout_value( cpv ), beneficiary_payout_value( bpv )
  {}

  account_name_type author; //author of the comment
  string            permlink; //permlink of the comment
  asset             payout; //(HBD) total value of comment reward recalculated to HBD
  share_type        author_rewards; //(HIVE satoshi) raw author reward (@see author_reward_operation) [is it needed?]
  asset             total_payout_value; //(HBD) overall author reward (from multiple cashouts prior to HF17) recalculated to HBD [is it needed?]
  asset             curator_payout_value; //(HBD) overall curation reward (from multiple cashouts prior to HF17) recalculated to HBD [is it needed?]
  asset             beneficiary_payout_value; //(HBD) overall beneficiary reward (from multiple cashouts prior to HF17) recalculated to HBD [is it needed?]
};

/**
  * Related to limit_order_create_operation and limit_order_create2_operation.
  * Generated during block processing to indicate reward paid to the market makers on internal HIVE<->HBD market.
  * No longer active after HF12.
  * @see fill_order_operation
  */
struct liquidity_reward_operation : public virtual_operation
{
  liquidity_reward_operation() = default;
  liquidity_reward_operation( const account_name_type& o, const asset& p )
    : owner( o ), payout( p )
  {}

  account_name_type owner; //market maker (receiver of payout)
  asset             payout; //(HIVE) reward for provided liquidity
};

/**
  * Related to any operation that modifies HBD liquid or savings balance (including block processing).
  * Generated when operation modified HBD balance and account received interest payment.
  * Interest is stored in related balance (liquid when liquid was modified, savings when savings was modified).
  * Note: since HF25 interest is not calculated nor paid on liquid balance.
  */
struct interest_operation : public virtual_operation
{
  interest_operation() = default;
  interest_operation( const account_name_type& o, const asset& i, bool _liquid_balance )
    :owner( o ), interest( i ), is_saved_into_hbd_balance( _liquid_balance )
  {}

  account_name_type owner; //user that had his HBD balance modified (receiver of interest)
  asset             interest; //(HBD) amount of interest paid
  bool              is_saved_into_hbd_balance; //true when liquid balance was modified (not happening after HF25)
};

/**
  * Related to withdraw_vesting_operation and set_withdraw_vesting_route_operation.
  * Generated during block processing in batches for each active withdraw route (including implied
  * from_account(VESTS)->from_account(HIVE)) each time vesting withdrawal period passes.
  * Note: not generated for implied route when all funds were already distributed along explicit routes
  */
struct fill_vesting_withdraw_operation : public virtual_operation
{
  fill_vesting_withdraw_operation() = default;
  fill_vesting_withdraw_operation( const account_name_type& f, const account_name_type& t, const asset& w, const asset& d )
    : from_account( f ), to_account( t ), withdrawn( w ), deposited( d )
  {}

  account_name_type from_account; //user that activated power down
  account_name_type to_account; //target of vesting route (potentially the same as from_account - receiver of deposited)
  asset             withdrawn; //(VESTS) source amount
  asset             deposited; //(HIVE or VESTS) [converted] target amount
};




  struct fill_collateralized_convert_request_operation : public virtual_operation
  {
    fill_collateralized_convert_request_operation() = default;
    fill_collateralized_convert_request_operation( const account_name_type& o, const uint32_t id,
      const HIVE_asset& in, const HBD_asset& out, const HIVE_asset& _excess_collateral )
      :owner( o ), requestid( id ), amount_in( in ), amount_out( out ), excess_collateral( _excess_collateral )
    {}

    account_name_type owner;
    uint32_t          requestid = 0;
    asset             amount_in; //in HIVE
    asset             amount_out; //in HBD
    asset             excess_collateral; //in HIVE
  };



  struct transfer_to_vesting_completed_operation : public virtual_operation
  {
    transfer_to_vesting_completed_operation(){}
    transfer_to_vesting_completed_operation( const string& f, const string& t, const asset& s, const asset& v )
      :from_account(f), to_account(t), hive_vested(s), vesting_shares_received(v) {}

    account_name_type from_account;
    account_name_type to_account;
    asset             hive_vested;
    asset             vesting_shares_received;
  };

  struct pow_reward_operation : public virtual_operation
  {
    pow_reward_operation(){}
    pow_reward_operation( const string& w, const asset& r )
      :worker(w), reward(r) {}

    account_name_type worker;
    asset             reward;
  };

  struct vesting_shares_split_operation : public virtual_operation
  {
    vesting_shares_split_operation(){}
    vesting_shares_split_operation( const string& o, const asset& old_vests, const asset& new_vests )
      :owner(o), vesting_shares_before_split(old_vests), vesting_shares_after_split(new_vests) {}

    account_name_type owner;
    asset             vesting_shares_before_split;
    asset             vesting_shares_after_split;
  };

  struct account_created_operation : public virtual_operation
  {
    account_created_operation() {}
    account_created_operation( const string& new_account_name, const string& creator, const asset& initial_vesting_shares, const asset& initial_delegation )
      :new_account_name(new_account_name), creator(creator), initial_vesting_shares(initial_vesting_shares), initial_delegation(initial_delegation)
    {
      FC_ASSERT(creator.size(), "Every account should have a creator");
    }

    account_name_type new_account_name;
    account_name_type creator;
    asset             initial_vesting_shares;
    asset             initial_delegation; // if created with account_create_with_delegation
  };


  struct shutdown_witness_operation : public virtual_operation
  {
    shutdown_witness_operation(){}
    shutdown_witness_operation( const string& o ):owner(o) {}

    account_name_type owner;
  };


  struct fill_order_operation : public virtual_operation
  {
    fill_order_operation(){}
    fill_order_operation( const string& c_o, uint32_t c_id, const asset& c_p, const string& o_o, uint32_t o_id, const asset& o_p )
    :current_owner(c_o), current_orderid(c_id), current_pays(c_p), open_owner(o_o), open_orderid(o_id), open_pays(o_p) {}

    account_name_type current_owner;
    uint32_t          current_orderid = 0;
    asset             current_pays;
    account_name_type open_owner;
    uint32_t          open_orderid = 0;
    asset             open_pays;
  };

  struct limit_order_cancelled_operation : public virtual_operation
  {
    limit_order_cancelled_operation() = default;
    limit_order_cancelled_operation(const string& _seller, uint32_t _order_id, const asset& _amount_back)
      :seller(_seller), orderid(_order_id), amount_back(_amount_back) {}

    account_name_type seller;
    uint32_t          orderid = 0;
    asset             amount_back;
  };

  struct fill_transfer_from_savings_operation : public virtual_operation
  {
    fill_transfer_from_savings_operation() {}
    fill_transfer_from_savings_operation( const account_name_type& f, const account_name_type& t, const asset& a, const uint32_t r, const string& m )
      :from(f), to(t), amount(a), request_id(r), memo(m) {}

    account_name_type from;
    account_name_type to;
    asset             amount;
    uint32_t          request_id = 0;
    string            memo;
  };

  struct hardfork_operation : public virtual_operation
  {
    hardfork_operation() {}
    hardfork_operation( uint32_t hf_id ) : hardfork_id( hf_id ) {}

    uint32_t         hardfork_id = 0;
  };

  struct comment_payout_update_operation : public virtual_operation
  {
    comment_payout_update_operation() {}
    comment_payout_update_operation( const account_name_type& a, const string& p ) : author( a ), permlink( p ) {}

    account_name_type author;
    string            permlink;
  };

  struct effective_comment_vote_operation : public virtual_operation
  {
    effective_comment_vote_operation() = default;
    effective_comment_vote_operation(const account_name_type& _voter, const account_name_type& _author,
      const string& _permlink, uint64_t _weight, int64_t _rshares, uint64_t _total_vote_weight)
    : voter(_voter), author(_author), permlink(_permlink), weight(_weight), rshares(_rshares),
      total_vote_weight(_total_vote_weight) {}

    account_name_type voter;
    account_name_type author;
    string            permlink;
    uint64_t          weight = 0; ///< defines the score this vote receives, used by vote payout calc. 0 if a negative vote or changed votes.
    int64_t           rshares = 0; ///< The number of rshares this vote is responsible for
    uint64_t          total_vote_weight = 0; ///< the total weight of voting rewards, used to calculate pro-rata share of curation payouts
    //potential payout of related comment at the moment of this vote
    asset             pending_payout = asset( 0, HBD_SYMBOL ); //supplemented by account history RocksDB plugin (needed by HiveMind)
  };

  struct ineffective_delete_comment_operation : public virtual_operation
  {
    ineffective_delete_comment_operation() = default;
    ineffective_delete_comment_operation(const account_name_type& _author, const string& _permlink) :
      author(_author), permlink(_permlink) {}

    account_name_type author;
    string            permlink;
  };

  struct return_vesting_delegation_operation : public virtual_operation
  {
    return_vesting_delegation_operation() {}
    return_vesting_delegation_operation( const account_name_type& a, const asset& v ) : account( a ), vesting_shares( v ) {}

    account_name_type account;
    asset             vesting_shares;
  };

  struct comment_benefactor_reward_operation : public virtual_operation
  {
    comment_benefactor_reward_operation() {}
    comment_benefactor_reward_operation( const account_name_type& b, const account_name_type& a,
      const string& p, const asset& s, const asset& st, const asset& v, bool must_be_claimed )
    : benefactor( b ), author( a ), permlink( p ), hbd_payout( s ), hive_payout( st ),
      vesting_payout( v ), payout_must_be_claimed( must_be_claimed )
    {}

    account_name_type benefactor;
    account_name_type author;
    string            permlink;
    asset             hbd_payout;
    asset             hive_payout;
    asset             vesting_payout;
    /// If set to true, payout has been stored in the separate reward balance, and must be claimed
    /// to be transferred to regular balance.
    bool              payout_must_be_claimed = false;
  };

  struct producer_reward_operation : public virtual_operation
  {
    producer_reward_operation(){}
    producer_reward_operation( const string& p, const asset& v ) : producer( p ), vesting_shares( v ) {}

    account_name_type producer;
    asset             vesting_shares;

  };

  struct clear_null_account_balance_operation : public virtual_operation
  {
    vector< asset >   total_cleared;
  };

  struct consolidate_treasury_balance_operation : public virtual_operation
  {
    vector< asset >   total_moved;
  };

  struct delayed_voting_operation : public virtual_operation
  {
    delayed_voting_operation(){}
    delayed_voting_operation( const account_name_type& _voter, const ushare_type _votes ) : voter( _voter ), votes( _votes ) {}

    account_name_type    voter;
    ushare_type          votes = 0;
  };

  struct dhf_funding_operation : public virtual_operation
  {
    dhf_funding_operation() {}
    dhf_funding_operation( const account_name_type& f, const asset& v ) : treasury( f ), additional_funds( v ) {}

    account_name_type treasury;
    asset additional_funds;
  };

  struct dhf_conversion_operation : public virtual_operation
  {
    dhf_conversion_operation() {}
    dhf_conversion_operation( account_name_type f, const asset& c, const asset& a ) : treasury( f ), hive_amount_in( c ), hbd_amount_out( a ) {}

    account_name_type treasury;
    asset hive_amount_in;
    asset hbd_amount_out;
  };

  // TODO : Fix legacy error itr != to_full_tag.end(): Invalid operation name: hardfork_hive {"n":"hardfork_hive"}
  struct hardfork_hive_operation : public virtual_operation
  {
    hardfork_hive_operation() {}
    //other_affected_accounts as well as assets transfered have to be filled during actual operation
    hardfork_hive_operation( const account_name_type& acc, const account_name_type& _treasury )
    : account( acc ), treasury( _treasury ), hbd_transferred( 0, HBD_SYMBOL ), hive_transferred( 0, HIVE_SYMBOL ),
      vests_converted( 0, VESTS_SYMBOL ), total_hive_from_vests( 0, HIVE_SYMBOL )
    {}

    account_name_type account;
    account_name_type treasury;
    std::vector< account_name_type >
                      other_affected_accounts; // delegatees that lost delegations from account - filled before pre notification
    asset             hbd_transferred; // filled only in post notification
    asset             hive_transferred; // filled only in post notification
    asset             vests_converted; // Amount of converted vests - filled only in post notification
    asset             total_hive_from_vests; // Resulting HIVE from conversion - filled only in post notification
  };

  struct hardfork_hive_restore_operation : public virtual_operation
  {
    hardfork_hive_restore_operation() {}
    hardfork_hive_restore_operation( const account_name_type& acc, const account_name_type& _treasury, const asset& s, const asset& st )
      : account( acc ), treasury( _treasury ), hbd_transferred( s ), hive_transferred( st ) {}

    account_name_type account;
    account_name_type treasury;
    asset             hbd_transferred;
    asset             hive_transferred;
  };

  struct expired_account_notification_operation : public virtual_operation
  {
    expired_account_notification_operation() = default;
    expired_account_notification_operation(const account_name_type& acc)
      : account(acc) {}

    account_name_type account;
  };

  struct changed_recovery_account_operation : public virtual_operation
  {
    changed_recovery_account_operation() = default;
    changed_recovery_account_operation( const account_name_type& acc, const account_name_type& oldrec, const account_name_type& newrec )
      : account( acc ), old_recovery_account( oldrec ), new_recovery_account( newrec ) {}

    account_name_type account;
    account_name_type old_recovery_account;
    account_name_type new_recovery_account;
  };

  struct system_warning_operation : public virtual_operation
  {
    system_warning_operation() = default;
    system_warning_operation( const string& _message )
      : message( _message ) {}

    string message;
  };

  struct fill_recurrent_transfer_operation : public virtual_operation
  {
    fill_recurrent_transfer_operation() {}
    fill_recurrent_transfer_operation( const account_name_type& f, const account_name_type& t, const asset& a, const string& m, uint16_t re )
      : from( f ), to( t ), amount( a ), memo( m ), remaining_executions( re ) {}

    account_name_type from;
    account_name_type to;
    asset amount;
    string memo;
    uint16_t remaining_executions = 0;
  };

  struct failed_recurrent_transfer_operation : public virtual_operation
  {
    failed_recurrent_transfer_operation() {}
    failed_recurrent_transfer_operation( const account_name_type& f, const account_name_type& t, const asset& a, uint8_t cf, const string& m, uint16_t re, bool d )
      : from( f ), to( t ), amount( a ), memo( m ), consecutive_failures( cf ), remaining_executions( re ), deleted( d ) {}

    account_name_type from;
    account_name_type to;
    asset amount;
    string memo;
    uint8_t consecutive_failures = 0;
    uint16_t remaining_executions = 0;
    bool deleted = false; // Indicates that the recurrent transfer was deleted due to too many consecutive failures
  };

  struct producer_missed_operation : public virtual_operation
  {
    producer_missed_operation() {}
    producer_missed_operation( const account_name_type& p ) : producer( p ) {}

    account_name_type producer;
  };

  struct proposal_fee_operation : public virtual_operation
  {
    proposal_fee_operation() {}
    proposal_fee_operation( const account_name_type& c, const account_name_type& t, uint32_t pid, const asset& f )
      : creator( c ), treasury( t ), proposal_id( pid ), fee( f ) {}

    account_name_type creator;
    account_name_type treasury;
    uint32_t proposal_id;
    asset fee;
  };

  struct collateralized_convert_immediate_conversion_operation : public virtual_operation
  {
    collateralized_convert_immediate_conversion_operation() {}
    collateralized_convert_immediate_conversion_operation( const account_name_type& o, uint32_t rid, const asset& out )
      : owner( o ), requestid( rid ), hbd_out( out ) {}

    account_name_type owner;
    uint32_t requestid;
    asset hbd_out;
  };

  struct escrow_approved_operation : public virtual_operation
  {
    escrow_approved_operation() {}
    escrow_approved_operation( const account_name_type& in, const account_name_type& out, const account_name_type& a, uint32_t eid, const asset& f )
      : from( in ), to( out ), agent( a ), escrow_id( eid ), fee( f )
    {}

    account_name_type from;
    account_name_type to;
    account_name_type agent;
    uint32_t escrow_id;
    asset fee;
  };

  struct escrow_rejected_operation : public virtual_operation
  {
    escrow_rejected_operation() {}
    escrow_rejected_operation( const account_name_type& in, const account_name_type& out, const account_name_type& a, uint32_t eid,
      const asset& d, const asset& h, const asset& f )
    : from( in ), to( out ), agent( a ), escrow_id( eid ), hbd_amount( d ), hive_amount( h ), fee( f )
    {}

    account_name_type from;
    account_name_type to;
    account_name_type agent;
    uint32_t escrow_id;
    asset hbd_amount;
    asset hive_amount;
    asset fee;
  };

} } //hive::protocol

FC_REFLECT( hive::protocol::fill_convert_request_operation, (owner)(requestid)(amount_in)(amount_out) )
FC_REFLECT( hive::protocol::author_reward_operation, (author)(permlink)(hbd_payout)(hive_payout)(vesting_payout)(curators_vesting_payout)(payout_must_be_claimed) )
FC_REFLECT( hive::protocol::curation_reward_operation, (curator)(reward)(comment_author)(comment_permlink)(payout_must_be_claimed) )
FC_REFLECT( hive::protocol::comment_reward_operation, (author)(permlink)(payout)(author_rewards)(total_payout_value)(curator_payout_value)(beneficiary_payout_value) )
FC_REFLECT( hive::protocol::liquidity_reward_operation, (owner)(payout) )
FC_REFLECT( hive::protocol::interest_operation, (owner)(interest)(is_saved_into_hbd_balance) )
FC_REFLECT( hive::protocol::fill_vesting_withdraw_operation, (from_account)(to_account)(withdrawn)(deposited) )
FC_REFLECT( hive::protocol::fill_collateralized_convert_request_operation, (owner)(requestid)(amount_in)(amount_out)(excess_collateral) )
FC_REFLECT( hive::protocol::account_created_operation, (new_account_name)(creator)(initial_vesting_shares)(initial_delegation) )
FC_REFLECT( hive::protocol::transfer_to_vesting_completed_operation, (from_account)(to_account)(hive_vested)(vesting_shares_received) )
FC_REFLECT( hive::protocol::pow_reward_operation, (worker)(reward) )
FC_REFLECT( hive::protocol::vesting_shares_split_operation, (owner)(vesting_shares_before_split)(vesting_shares_after_split) )
FC_REFLECT( hive::protocol::shutdown_witness_operation, (owner) )
FC_REFLECT( hive::protocol::fill_order_operation, (current_owner)(current_orderid)(current_pays)(open_owner)(open_orderid)(open_pays) )
FC_REFLECT( hive::protocol::limit_order_cancelled_operation, (seller)(amount_back))
FC_REFLECT( hive::protocol::fill_transfer_from_savings_operation, (from)(to)(amount)(request_id)(memo) )
FC_REFLECT( hive::protocol::hardfork_operation, (hardfork_id) )
FC_REFLECT( hive::protocol::comment_payout_update_operation, (author)(permlink) )
FC_REFLECT( hive::protocol::effective_comment_vote_operation, (voter)(author)(permlink)(weight)(rshares)(total_vote_weight)(pending_payout))
FC_REFLECT( hive::protocol::ineffective_delete_comment_operation, (author)(permlink))
FC_REFLECT( hive::protocol::return_vesting_delegation_operation, (account)(vesting_shares) )
FC_REFLECT( hive::protocol::comment_benefactor_reward_operation, (benefactor)(author)(permlink)(hbd_payout)(hive_payout)(vesting_payout)(payout_must_be_claimed) )
FC_REFLECT( hive::protocol::producer_reward_operation, (producer)(vesting_shares) )
FC_REFLECT( hive::protocol::clear_null_account_balance_operation, (total_cleared) )
FC_REFLECT( hive::protocol::consolidate_treasury_balance_operation, ( total_moved ) )
FC_REFLECT( hive::protocol::delayed_voting_operation, (voter)(votes) )
FC_REFLECT( hive::protocol::dhf_funding_operation, (treasury)(additional_funds) )
FC_REFLECT( hive::protocol::dhf_conversion_operation, (treasury)(hive_amount_in)(hbd_amount_out) )
FC_REFLECT( hive::protocol::hardfork_hive_operation, (account)(treasury)(other_affected_accounts)(hbd_transferred)(hive_transferred)(vests_converted)(total_hive_from_vests) )
FC_REFLECT( hive::protocol::hardfork_hive_restore_operation, (account)(treasury)(hbd_transferred)(hive_transferred) )
FC_REFLECT( hive::protocol::expired_account_notification_operation, (account) )
FC_REFLECT( hive::protocol::changed_recovery_account_operation, (account)(old_recovery_account)(new_recovery_account) )
FC_REFLECT( hive::protocol::system_warning_operation, (message) )
FC_REFLECT( hive::protocol::fill_recurrent_transfer_operation, (from)(to)(amount)(memo)(remaining_executions) )
FC_REFLECT( hive::protocol::failed_recurrent_transfer_operation, (from)(to)(amount)(memo)(consecutive_failures)(remaining_executions)(deleted) )
FC_REFLECT( hive::protocol::producer_missed_operation, (producer) )
FC_REFLECT( hive::protocol::proposal_fee_operation, (creator)(treasury)(proposal_id)(fee) )
FC_REFLECT( hive::protocol::collateralized_convert_immediate_conversion_operation, (owner)(requestid)(hbd_out) )
FC_REFLECT( hive::protocol::escrow_approved_operation, (from)(to)(agent)(escrow_id)(fee) )
FC_REFLECT( hive::protocol::escrow_rejected_operation, (from)(to)(agent)(escrow_id)(hbd_amount)(hive_amount)(fee) )

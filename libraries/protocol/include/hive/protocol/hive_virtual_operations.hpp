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

/**
  * Related to limit_order_create_operation and limit_order_create2_operation.
  * Generated during one of above operations when order on internal market is partially or fully matched
  * (each match generates separate vop).
  */
struct fill_order_operation : public virtual_operation
{
  fill_order_operation() = default;
  fill_order_operation( const account_name_type& c_o, uint32_t c_id, const asset& c_p, const account_name_type& o_o, uint32_t o_id, const asset& o_p )
    : current_owner( c_o ), current_orderid( c_id ), current_pays( c_p ), open_owner( o_o ), open_orderid( o_id ), open_pays( o_p )
  {}

  account_name_type current_owner; //user that recently created order (taker - receiver of open_pays)
  uint32_t          current_orderid = 0; //id of fresh order
  asset             current_pays; //(HIVE or HBD) amount paid to open_owner
  account_name_type open_owner; //user that had his order on the market (maker - receiver of current_pays)
  uint32_t          open_orderid = 0; //id of waiting order
  asset             open_pays; //(HBD or HIVE) amount paid to current_owner
};

/**
  * Related to block processing.
  * Generated during block processing when witness is removed from active witnesses after it was detected he have missed
  * all blocks scheduled for him for last day. No longer active after HF20.
  * @see producer_missed_operation
  */
struct shutdown_witness_operation : public virtual_operation
{
  shutdown_witness_operation() = default;
  shutdown_witness_operation( const account_name_type& o )
    : owner( o )
  {}

  account_name_type owner; //witness that was shut down
};

/**
  * Related to transfer_from_savings_operation.
  * Generated during block processing after savings withdraw time has passed and requested amount
  * was transfered from savings to liquid balance.
  */
struct fill_transfer_from_savings_operation : public virtual_operation
{
  fill_transfer_from_savings_operation() = default;
  fill_transfer_from_savings_operation( const account_name_type& f, const account_name_type& t, const asset& a, const uint32_t r, const string& m )
    : from( f ), to( t ), amount( a ), request_id( r ), memo( m )
  {}

  account_name_type from; //user that initiated transfer from savings
  account_name_type to; //user that was specified to receive funds (receiver of amount)
  asset             amount; //(HIVE or HBD) funds transfered from savings
  uint32_t          request_id = 0; //id of transfer request
  string            memo; //memo attached to transfer request
};

/**
  * Related to block processing.
  * Generated during block processing every time new hardfork is activated. Many related vops can follow.
  */
struct hardfork_operation : public virtual_operation
{
  hardfork_operation() = default;
  hardfork_operation( uint32_t hf_id )
    : hardfork_id( hf_id )
  {}

  uint32_t          hardfork_id = 0; //number of hardfork
};

/**
  * Related to comment_operation.
  * Generated during block processing after cashout time passes even if there are no rewards.
  * Note: prior to HF17 comment could have multiple cashout windows.
  */
struct comment_payout_update_operation : public virtual_operation
{
  comment_payout_update_operation() = default;
  comment_payout_update_operation( const account_name_type& a, const string& p )
    : author( a ), permlink( p )
  {}

  account_name_type author; //author of comment
  string            permlink; //permlink of comment
};

/**
  * Related to delegate_vesting_shares_operation.
  * Generated during block processing when process of returning removed or lowered vesting delegation is finished (after return period
  * passed) and delegator received back his vests.
  */
struct return_vesting_delegation_operation : public virtual_operation
{
  return_vesting_delegation_operation() = default;
  return_vesting_delegation_operation( const account_name_type& a, const asset& v )
    : account( a ), vesting_shares( v )
  {}

  account_name_type account; //delegator (receiver of vesting_shares)
  asset             vesting_shares; //(VESTS) returned delegation
};

/**
  * Related to comment_operation and comment_options_operation.
  * Generated during block processing after cashout time passes and comment is eligible for rewards (nonzero reward).
  * Note: the reward is a fragment of the author portion of comment reward depending on share assigned to benefactor
  * in comment options (can be zero due to rounding).
  * @see author_reward_operation
  */
struct comment_benefactor_reward_operation : public virtual_operation
{
  comment_benefactor_reward_operation() = default;
  comment_benefactor_reward_operation( const account_name_type& b, const account_name_type& a,
    const string& p, const asset& s, const asset& st, const asset& v, bool must_be_claimed )
    : benefactor( b ), author( a ), permlink( p ), hbd_payout( s ), hive_payout( st ),
    vesting_payout( v ), payout_must_be_claimed( must_be_claimed )
  {}

  account_name_type benefactor; //user assigned to receive share of author reward (receiver of payouts)
  account_name_type author; //author of the comment
  string            permlink; //permlink of the comment
  asset             hbd_payout; //(HBD) part of reward
  asset             hive_payout; //(HIVE) part of reward
  asset             vesting_payout; //(VESTS) part of reward
  bool              payout_must_be_claimed = false; //true if payouts require use of claim_reward_balance_operation
};

/**
  * Related to block processing.
  * Generated during block processing every block for current witness.
  */
struct producer_reward_operation : public virtual_operation
{
  producer_reward_operation() = default;
  producer_reward_operation( const account_name_type& p, const asset& v )
    : producer( p ), vesting_shares( v )
  {}

  account_name_type producer; //witness (receiver of vesting_shares)
  asset             vesting_shares; //(VESTS or HIVE) reward for block production (HIVE only during first 30 days after genesis)
};

/**
  * Related to block processing.
  * Generated during block processing potentially every block, but only if nonzero assets were burned. Triggered by removal of all
  * assets from 'null' account balances.
  */
struct clear_null_account_balance_operation : public virtual_operation
{
  vector< asset >   total_cleared; //(HIVE, VESTS or HBD) nonzero assets burned on 'null' account
};

/**
  * Related to create_proposal_operation.
  * Generated during block processing during proposal maintenance in batches
  * for each proposal that is chosen and receives funding.
  */
struct proposal_pay_operation : public virtual_operation
{
  proposal_pay_operation() = default;
  proposal_pay_operation( uint32_t _proposal_id, const account_name_type& _receiver, const account_name_type& _treasury, const asset& _payment,
    const transaction_id_type& _trx_id, uint16_t _op_in_trx )
    : proposal_id( _proposal_id ), receiver( _receiver ), payer( _treasury ), payment( _payment ), trx_id( _trx_id ), op_in_trx( _op_in_trx )
  {}

  uint32_t          proposal_id = 0; //id of chosen proposal
  account_name_type receiver; //account designated to receive funding (receiver of payment)
  account_name_type payer; //treasury account, source of payment
  asset             payment; //(HBD) paid amount

  transaction_id_type trx_id; //id of transaction with proposal [is it needed? does not look like the value description is correct]
  uint16_t          op_in_trx = 0; //operation index for proposal within above transaction [is it needed? does not look like the value description is correct]
};

/**
  * Related to block processing.
  * Generated during block processing every proposal maintenance period.
  * Note: while the fund receives part of inflation every block, the amount is recorded aside and only when there are
  * proposal payouts (when new funds matter), there is generation of this vop.
  */
struct dhf_funding_operation : public virtual_operation
{
  dhf_funding_operation() = default;
  dhf_funding_operation( const account_name_type& f, const asset& v )
    : treasury( f ), additional_funds( v )
  {}

  account_name_type treasury; //treasury account (receiver of additional_funds)
  asset             additional_funds; //(HBD) portion inflation accumulated since previous maintenance period
};

/**
  * Related to hardfork 23 (HIVE inception hardfork).
  * Generated for every account that did not receive HIVE airdrop.
  */
struct hardfork_hive_operation : public virtual_operation
{
  hardfork_hive_operation() = default;
  //other_affected_accounts as well as assets transfered have to be filled during actual operation
  hardfork_hive_operation( const account_name_type& acc, const account_name_type& _treasury )
    : account( acc ), treasury( _treasury ), hbd_transferred( 0, HBD_SYMBOL ), hive_transferred( 0, HIVE_SYMBOL ),
    vests_converted( 0, VESTS_SYMBOL ), total_hive_from_vests( 0, HIVE_SYMBOL )
  {}

  account_name_type account; //account excluded from airdrop (source of amounts for airdrop)
  account_name_type treasury; //treasury that received airdrop instead of account (receiver of funds)
  std::vector< account_name_type >
                    other_affected_accounts; //delegatees that lost delegations from account - filled before pre notification
  asset             hbd_transferred; //(HBD) part of airdrop to treasury (sourced from various HBD balances on account)
  asset             hive_transferred; //(HIVE) part of airdrop to treasury (sourced from various HIVE balances on account)
  asset             vests_converted; //(VESTS) sum of all sources of VESTS on account
  asset             total_hive_from_vests; //(HIVE) part of airdrop to treasury (sourced from conversion of vests_converted)
};

/**
  * Related to hardfork 24.
  * Generated for every account that was excluded from HF23 airdrop but won appeal.
  * Note: the late airdrop did not apply properly since HIVE that the accounts should receive did not account for
  * HIVE from converted VESTS. [how was it resolved?]
  * @see hardfork_hive_operation
  */
struct hardfork_hive_restore_operation : public virtual_operation
{
  hardfork_hive_restore_operation() = default;
  hardfork_hive_restore_operation( const account_name_type& acc, const account_name_type& _treasury, const asset& s, const asset& st )
    : account( acc ), treasury( _treasury ), hbd_transferred( s ), hive_transferred( st )
  {}

  account_name_type account; //account to receive late airdrop (receiver of funds)
  account_name_type treasury; //treasury, source of late airdrop
  asset             hbd_transferred; //(HBD) part of airdrop (equals related hardfork_hive_operation.hbd_transferred)
  asset             hive_transferred; //(HIVE) part of airdrop (equals related hardfork_hive_operation.hive_transferred)
};

/**
  * Related to transfer_to_vesting_operation.
  * Generated during block processing every time part of fairly fresh VESTS becomes active part of governance vote for the account.
  * Note: after account receives new VESTS there is a grace period before those VESTS are accounted for when
  * it comes to governance vote power. This vop is generated at the end of that period.
  */
struct delayed_voting_operation : public virtual_operation
{
  delayed_voting_operation() = default;
  delayed_voting_operation( const account_name_type& _voter, const ushare_type _votes )
    : voter( _voter ), votes( _votes )
  {}

  account_name_type voter; //account with fairly fresh VESTS
  ushare_type       votes = 0; //(VESTS satoshi) new governance vote power that just activated for voter
};

/**
  * Related to block processing.
  * Generated during block processing potentially every block, but only if there is nonzero transfer. Transfer occurs
  * if there are assets on OBSOLETE_TREASURY_ACCOUNT ('steem.dao'). They are consolidated from all balances (per asset
  * type) and moved to NEW_HIVE_TREASURY_ACCOUNT ('hive.fund').
  */
struct consolidate_treasury_balance_operation : public virtual_operation
{
  vector< asset >   total_moved; //(HIVE, VESTS or HBD) funds moved from old to new treasury
};

/**
  * Related to vote_operation.
  * Generated every time vote is cast for the first time or edited, but only as long as it is effective, that is,
  * the target comment was not yet cashed out.
  */
struct effective_comment_vote_operation : public virtual_operation
{
  effective_comment_vote_operation() = default;
  effective_comment_vote_operation( const account_name_type& _voter, const account_name_type& _author,
    const string& _permlink, uint64_t _weight, int64_t _rshares, uint64_t _total_vote_weight )
    : voter( _voter ), author( _author ), permlink( _permlink ), weight( _weight ), rshares( _rshares ),
    total_vote_weight( _total_vote_weight )
  {}

  account_name_type voter; //account that casts a vote
  account_name_type author; //author of comment voted on
  string            permlink; //permlink of comment voted on
  uint64_t          weight = 0; //weight of vote depending on when vote was cast and with what power
  int64_t           rshares = 0; //power of the vote
  uint64_t          total_vote_weight = 0; //sum of all vote weights on the target comment in the moment of casting current vote
  asset             pending_payout = asset( 0, HBD_SYMBOL ); //(HBD) estimated reward on target comment; supplemented by AH RocksDB plugin
};

/**
  * Related to delete_comment_operation.
  * Generated when delete_comment_operation was executed but ignored.
  * Note: prior to HF19 it was possible to execute delete on comment that had net positive votes. Such operation was ignored.
  * This is the moment this vop is generated.
  */
struct ineffective_delete_comment_operation : public virtual_operation
{
  ineffective_delete_comment_operation() = default;
  ineffective_delete_comment_operation( const account_name_type& _author, const string& _permlink )
    : author( _author ), permlink( _permlink )
  {}

  account_name_type author; //author of attempted-delete comment
  string            permlink; //permlink of attempted-delete comment
};

/**
  * Related to specific case of transfer_operation and to block processing.
  * When user transferred HIVE to treasury the amount is immediately converted to HBD and this vops is generated.
  * Also generated during block processing every day during daily proposal maintenance.
  * Note: portion of HIVE on treasury balance is converted to HBD and thus increases funds available for proposals.
  */
struct dhf_conversion_operation : public virtual_operation
{
  dhf_conversion_operation() = default;
  dhf_conversion_operation( const account_name_type& f, const asset& c, const asset& a )
    : treasury( f ), hive_amount_in( c ), hbd_amount_out( a )
  {}

  account_name_type treasury; //treasury (source of hive_amount_in and receiver of hbd_amount_out)
  asset             hive_amount_in; //(HIVE) source of conversion
  asset             hbd_amount_out; //(HBD) effect of conversion
};

/**
  * Related to governance voting: account_witness_vote_operation, account_witness_proxy_operation and update_proposal_votes_operation.
  * Generated during block processing when user did not cast any governance vote for very long time. Such user is considered not
  * interested in governance and therefore his previous votes are nullified.
  */
struct expired_account_notification_operation : public virtual_operation
{
  expired_account_notification_operation() = default;
  expired_account_notification_operation( const account_name_type& acc )
    : account( acc )
  {}

  account_name_type account; //user whose governance votes were nullified
};

/**
  * Related to change_recovery_account_operation.
  * Generated during block processing after wait period for the recovery account change has passed and the change became active.
  */
struct changed_recovery_account_operation : public virtual_operation
{
  changed_recovery_account_operation() = default;
  changed_recovery_account_operation( const account_name_type& acc, const account_name_type& oldrec, const account_name_type& newrec )
    : account( acc ), old_recovery_account( oldrec ), new_recovery_account( newrec )
  {}

  account_name_type account; //used that requested recovery accout change
  account_name_type old_recovery_account; //previous recovery account
  account_name_type new_recovery_account; //new recovery account
};

/**
  * Related to transfer_to_vesting_operation.
  * Generated every time above operation is executed. Supplements it with amount of VESTS received.
  * Note: power up immediately increases mana regeneration and vote power for comments, but there is a grace period before
  * it activates as governance vote power.
  * @see delayed_voting_operation
  */
struct transfer_to_vesting_completed_operation : public virtual_operation
{
  transfer_to_vesting_completed_operation() = default;
  transfer_to_vesting_completed_operation( const account_name_type& f, const account_name_type& t, const asset& s, const asset& v )
    : from_account( f ), to_account( t ), hive_vested( s ), vesting_shares_received( v )
  {}

  account_name_type from_account; //account that executed power up (source of hive_vested)
  account_name_type to_account; //account that gets power up (receiver of vesting_shares_received)
  asset             hive_vested; //(HIVE) liquid funds being turned into VESTS
  asset             vesting_shares_received; //(VESTS) result of power up
};

/**
  * Related to pow_operation and pow2_operation.
  * Generated every time one of above operations is executed (up to HF16).
  * Note: pow2_operation could be executed up to HF17 but mining rewards were stopped after HF16.
  */
struct pow_reward_operation : public virtual_operation
{
  pow_reward_operation() = default;
  pow_reward_operation( const account_name_type& w, const asset& r )
    : worker( w ), reward( r )
  {}

  account_name_type worker; //(potentially new) witness that calculated PoW (receiver of reward)
  asset             reward; //(VESTS or HIVE) reward for work (HIVE only during first 30 days after genesis)
};

/**
  * Related to hardfork 1.
  * Generated for every account with nonzero vesting balance.
  * Note: due to too small precision of VESTS asset it was increased by 6 digits, meaning all underlying
  * amounts had to be multiplied by million.
  */
struct vesting_shares_split_operation : public virtual_operation
{
  vesting_shares_split_operation() = default;
  vesting_shares_split_operation( const account_name_type& o, const asset& old_vests, const asset& new_vests )
    : owner( o ), vesting_shares_before_split( old_vests ), vesting_shares_after_split( new_vests )
  {}

  account_name_type owner; //affected account (source of vesting_shares_before_split and receiver of vesting_shares_after_split)
  asset             vesting_shares_before_split; //(VESTS) balance before split
  asset             vesting_shares_after_split; //(VESTS) balance after split
};

/**
  * Related to all acts of account creation, that is, creation of genesis accounts as well as operations:
  * account_create_operation, account_create_with_delegation_operation, pow_operation, pow2_operation and create_claimed_account_operation.
  * Generated every time one of above operations results in creation of new account (account is always created except for pow/pow2).
  * Note: vops for genesis accounts are generated at the start of block #1.
  */
struct account_created_operation : public virtual_operation
{
  account_created_operation() = default;
  account_created_operation( const account_name_type& new_account_name, const account_name_type& creator, const asset& _initial_vesting, const asset& _delegation )
    : new_account_name( new_account_name ), creator( creator ), initial_vesting_shares( _initial_vesting ), initial_delegation( _delegation )
  {
    FC_ASSERT( creator != account_name_type(), "Every account should have a creator" );
  }

  account_name_type new_account_name; //newly created account (receiver of initial_vesting_shares)
  account_name_type creator; //account that initiated new account creation (genesis and mined accounts are their own creators)
  asset             initial_vesting_shares; //(VESTS) amount of initial vesting on new account (converted from creation fee prior to HF20)
  asset             initial_delegation; //(VESTS) amount of extra voting power on new account due to delegation
};

/**
  * Related to collateralized_convert_operation.
  * Generated during block processing after conversion delay passes and HIVE is finally converted to HBD.
  * Note: HBD is transferred immediately during execution of above operation, this vop is generated after actual
  * price of conversion becomes known.
  * @see collateralized_convert_immediate_conversion_operation
  */
struct fill_collateralized_convert_request_operation : public virtual_operation
{
  fill_collateralized_convert_request_operation() = default;
  fill_collateralized_convert_request_operation( const account_name_type& o, const uint32_t id,
    const HIVE_asset& in, const HBD_asset& out, const HIVE_asset& _excess_collateral )
    : owner( o ), requestid( id ), amount_in( in ), amount_out( out ), excess_collateral( _excess_collateral )
  {}

  account_name_type owner; //user that requested conversion (receiver of excess_collateral)
  uint32_t          requestid = 0; //id of the request
  asset             amount_in; //(HIVE) source of conversion (part of collateral)
  asset             amount_out; //(HBD) result of conversion (already transferred to owner when request was made)
  asset             excess_collateral; //(HIVE) unused part of collateral returned to owner
};

/**
  * Related to block processing or selected operations.
  * Generated every time something occurs that would normally be only visible to node operators in their logs
  * but might be interesting to general HIVE community. Such vops can be observed on account history of 'initminer'.
  * Currently the following generate system warnings:
  *  - unknown type of witness during block processing [should probably be FC_ASSERT]
  *    indicates some problem in the code
  *  - shortfall of collateral during finalization of HIVE->HBD conversion (@see fill_collateralized_convert_request_operation)
  *    the community covers the difference in form of tiny amount of extra inflation
  *  - artificial correction of internal price of HIVE due to hitting of HBD hard cap limit
  *    every operation that involves conversion from HBD to HIVE will give output amount that is smaller than real world value
  *  - noncanonical fee symbol used by witness [should disappear if it never happened as suggested by TODO message]
  *  - witnesses changed maximum block size
  */
struct system_warning_operation : public virtual_operation
{
  system_warning_operation() = default;
  system_warning_operation( const string& _message )
    : message( _message )
  {}

  string            message; //warning message
};

/**
  * Related to recurrent_transfer_operation.
  * Generated during block processing starting in the block that included above operation and then after every period
  * set in the operation until all transfers are executed, too many fail due to shortfall of funds or the transfer is cancelled.
  * Note: in case of accumulation of very big amount of recurrent transfers to be executed in particular block, some
  * are going to be postponed to next block(s) and so will be generation of this vop.
  * @see failed_recurrent_transfer_operation
  */
struct fill_recurrent_transfer_operation : public virtual_operation
{
  fill_recurrent_transfer_operation() = default;
  fill_recurrent_transfer_operation( const account_name_type& f, const account_name_type& t, const asset& a, const string& m, uint16_t re )
    : from( f ), to( t ), amount( a ), memo( m ), remaining_executions( re )
  {}

  account_name_type from; //user that initiated the transfer (source of amount)
  account_name_type to; //user that is target of transfer (receiver of amount)
  asset             amount; //(HIVE of HBD) amount transferred in current iteration
  string            memo; //memo attached to the transfer
  uint16_t          remaining_executions = 0; //number of remaining pending transfers
};

/**
  * Related to recurrent_transfer_operation.
  * Generated during block processing instead of fill_recurrent_transfer_operation when there is not enought funds on from account.
  * Note: failed transfers are not automatically repeated.
  * Note: if too many consecutive transfers fail, whole recurrent transfer operation is discontinued.
  * @see fill_recurrent_transfer_operation
  */
struct failed_recurrent_transfer_operation : public virtual_operation
{
  failed_recurrent_transfer_operation() = default;
  failed_recurrent_transfer_operation( const account_name_type& f, const account_name_type& t, const asset& a, uint8_t cf, const string& m, uint16_t re, bool d )
    : from( f ), to( t ), amount( a ), memo( m ), consecutive_failures( cf ), remaining_executions( re ), deleted( d )
  {}

  account_name_type from; //user that initiated the transfer (source of amount that has not enough balance to cover it)
  account_name_type to; //user that is target of transfer (would be receiver of amount, but no transfer actually happened)
  asset             amount; //(HIVE of HBD) amount that was scheduled for transferred in current iteration but failed
  string            memo; //memo attached to the transfer
  uint8_t           consecutive_failures = 0; //number of failed iterations
  uint16_t          remaining_executions = 0; //number of remaining pending transfers
  bool              deleted = false; //true if whole recurrent transfer was discontinued due to too many consecutive failures
};

/**
  * Related to limit_order_cancel_operation, limit_order_create_operation or limit_order_create2_operation.
  * Generated every time existing limit order is cancelled. It happens on explicit call (first operation), or in rare case of
  * filling limit order (second or third operation) when, after filling most of it, remaining funds are too small (would round
  * to zero when sold). Finally also generated during block processing for orders that reached expiration time without being filled.
  * @see fill_order_operation
  */
struct limit_order_cancelled_operation : public virtual_operation
{
  limit_order_cancelled_operation() = default;
  limit_order_cancelled_operation( const account_name_type& _seller, uint32_t _order_id, const asset& _amount_back )
    : seller( _seller ), orderid( _order_id ), amount_back( _amount_back )
  {}

  account_name_type seller; //user that placed an order (receiver of amount_back)
  uint32_t          orderid = 0; //id of the order
  asset             amount_back; //(HIVE or HBD) remaining funds from original order that were not traded until cancellation
};

/**
  * Related to block processing.
  * Generated during block processing when witness failed to produce his block on time.
  * @see shutdown_witness_operation
  */
struct producer_missed_operation : public virtual_operation
{
  producer_missed_operation() = default;
  producer_missed_operation( const account_name_type& p )
    : producer( p )
  {}

  account_name_type producer; //witness that failed to produce his block on time
};

/**
  * Related to create_proposal_operation.
  * Generated every time above operation is executed. Supplements it with paid fee.
  */
struct proposal_fee_operation : public virtual_operation
{
  proposal_fee_operation() = default;
  proposal_fee_operation( const account_name_type& c, const account_name_type& t, uint32_t pid, const asset& f )
    : creator( c ), treasury( t ), proposal_id( pid ), fee( f )
  {}

  account_name_type creator; //user that created proposal (source of fee)
  account_name_type treasury; //treasury account (receiver of fee)
  uint32_t          proposal_id; //id of proposal
  asset             fee; //(HBD) amount paid for proposal [should actually be part of create_proposal_operation but it's too late now]
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
FC_REFLECT( hive::protocol::fill_order_operation, (current_owner)(current_orderid)(current_pays)(open_owner)(open_orderid)(open_pays) )
FC_REFLECT( hive::protocol::shutdown_witness_operation, (owner) )
FC_REFLECT( hive::protocol::fill_transfer_from_savings_operation, (from)(to)(amount)(request_id)(memo) )
FC_REFLECT( hive::protocol::hardfork_operation, (hardfork_id) )
FC_REFLECT( hive::protocol::comment_payout_update_operation, (author)(permlink) )
FC_REFLECT( hive::protocol::return_vesting_delegation_operation, (account)(vesting_shares) )
FC_REFLECT( hive::protocol::comment_benefactor_reward_operation, (benefactor)(author)(permlink)(hbd_payout)(hive_payout)(vesting_payout)(payout_must_be_claimed) )
FC_REFLECT( hive::protocol::producer_reward_operation, (producer)(vesting_shares) )
FC_REFLECT( hive::protocol::clear_null_account_balance_operation, (total_cleared) )
FC_REFLECT( hive::protocol::proposal_pay_operation, (proposal_id)(receiver)(payer)(payment)(trx_id)(op_in_trx) )
FC_REFLECT( hive::protocol::dhf_funding_operation, (treasury)(additional_funds) )
FC_REFLECT( hive::protocol::hardfork_hive_operation, (account)(treasury)(other_affected_accounts)(hbd_transferred)(hive_transferred)(vests_converted)(total_hive_from_vests) )
FC_REFLECT( hive::protocol::hardfork_hive_restore_operation, (account)(treasury)(hbd_transferred)(hive_transferred) )
FC_REFLECT( hive::protocol::delayed_voting_operation, (voter)(votes) )
FC_REFLECT( hive::protocol::consolidate_treasury_balance_operation, (total_moved) )
FC_REFLECT( hive::protocol::effective_comment_vote_operation, (voter)(author)(permlink)(weight)(rshares)(total_vote_weight)(pending_payout) )
FC_REFLECT( hive::protocol::ineffective_delete_comment_operation, (author)(permlink) )
FC_REFLECT( hive::protocol::dhf_conversion_operation, (treasury)(hive_amount_in)(hbd_amount_out) )
FC_REFLECT( hive::protocol::expired_account_notification_operation, (account) )
FC_REFLECT( hive::protocol::changed_recovery_account_operation, (account)(old_recovery_account)(new_recovery_account) )
FC_REFLECT( hive::protocol::transfer_to_vesting_completed_operation, (from_account)(to_account)(hive_vested)(vesting_shares_received) )
FC_REFLECT( hive::protocol::pow_reward_operation, (worker)(reward) )
FC_REFLECT( hive::protocol::vesting_shares_split_operation, (owner)(vesting_shares_before_split)(vesting_shares_after_split) )
FC_REFLECT( hive::protocol::account_created_operation, (new_account_name)(creator)(initial_vesting_shares)(initial_delegation) )
FC_REFLECT( hive::protocol::fill_collateralized_convert_request_operation, (owner)(requestid)(amount_in)(amount_out)(excess_collateral) )
FC_REFLECT( hive::protocol::system_warning_operation, (message) )
FC_REFLECT( hive::protocol::fill_recurrent_transfer_operation, (from)(to)(amount)(memo)(remaining_executions) )
FC_REFLECT( hive::protocol::failed_recurrent_transfer_operation, (from)(to)(amount)(memo)(consecutive_failures)(remaining_executions)(deleted) )
FC_REFLECT( hive::protocol::limit_order_cancelled_operation, (seller)(amount_back) )
FC_REFLECT( hive::protocol::producer_missed_operation, (producer) )
FC_REFLECT( hive::protocol::proposal_fee_operation, (creator)(treasury)(proposal_id)(fee) )
FC_REFLECT( hive::protocol::collateralized_convert_immediate_conversion_operation, (owner)(requestid)(hbd_out) )
FC_REFLECT( hive::protocol::escrow_approved_operation, (from)(to)(agent)(escrow_id)(fee) )
FC_REFLECT( hive::protocol::escrow_rejected_operation, (from)(to)(agent)(escrow_id)(hbd_amount)(hive_amount)(fee) )

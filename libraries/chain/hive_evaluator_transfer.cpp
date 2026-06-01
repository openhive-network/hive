#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/fixed_string.hpp>

#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_virtual_operations.hpp>
#include <hive/chain/rc/rc_utility.hpp>
#include <hive/chain/detail/state/account_object_multiindex.hpp>
#include <hive/chain/detail/state/convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/collateralized_convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/feed_history_object.hpp>
#include <hive/chain/detail/state/escrow_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>
#include <hive/chain/detail/state/savings_withdraw_object_multiindex.hpp>
#include <hive/chain/detail/state/withdraw_vesting_route_object_multiindex.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object_multiindex.hpp>
#include <hive/chain/detail/state/witness_objects.hpp>
#include <hive/chain/detail/state/global_property_object_multiindex.hpp>
#include <hive/chain/evaluator_registry.hpp>

#include <hive/chain/chain_validation.hpp>

#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/manabar.hpp>
#include <hive/chain/util/delayed_voting.hpp>

#include <fc/uint128.hpp>

namespace hive { namespace chain {

HIVE_DEFINE_EVALUATOR( transfer )
HIVE_DEFINE_EVALUATOR( transfer_to_vesting )
HIVE_DEFINE_EVALUATOR( withdraw_vesting )
HIVE_DEFINE_EVALUATOR( set_withdraw_vesting_route )
HIVE_DEFINE_EVALUATOR( convert )
HIVE_DEFINE_EVALUATOR( collateralized_convert )
HIVE_DEFINE_EVALUATOR( limit_order_create )
HIVE_DEFINE_EVALUATOR( limit_order_create2 )
HIVE_DEFINE_EVALUATOR( limit_order_cancel )
HIVE_DEFINE_EVALUATOR( escrow_transfer )
HIVE_DEFINE_EVALUATOR( escrow_approve )
HIVE_DEFINE_EVALUATOR( escrow_dispute )
HIVE_DEFINE_EVALUATOR( escrow_release )
HIVE_DEFINE_EVALUATOR( transfer_to_savings )
HIVE_DEFINE_EVALUATOR( transfer_from_savings )
HIVE_DEFINE_EVALUATOR( cancel_transfer_from_savings )
HIVE_DEFINE_EVALUATOR( claim_reward_balance )
HIVE_DEFINE_EVALUATOR( delegate_vesting_shares )
HIVE_DEFINE_EVALUATOR( recurrent_transfer )

void register_transfer_evaluators( evaluator_registry<operation>& registry )
{
  registry.register_evaluator< transfer_evaluator                       >();
  registry.register_evaluator< transfer_to_vesting_evaluator            >();
  registry.register_evaluator< withdraw_vesting_evaluator               >();
  registry.register_evaluator< set_withdraw_vesting_route_evaluator     >();
  registry.register_evaluator< convert_evaluator                        >();
  registry.register_evaluator< collateralized_convert_evaluator         >();
  registry.register_evaluator< limit_order_create_evaluator             >();
  registry.register_evaluator< limit_order_create2_evaluator            >();
  registry.register_evaluator< limit_order_cancel_evaluator             >();
  registry.register_evaluator< escrow_transfer_evaluator                >();
  registry.register_evaluator< escrow_approve_evaluator                 >();
  registry.register_evaluator< escrow_dispute_evaluator                 >();
  registry.register_evaluator< escrow_release_evaluator                 >();
  registry.register_evaluator< transfer_to_savings_evaluator            >();
  registry.register_evaluator< transfer_from_savings_evaluator          >();
  registry.register_evaluator< cancel_transfer_from_savings_evaluator   >();
  registry.register_evaluator< claim_reward_balance_evaluator           >();
  registry.register_evaluator< delegate_vesting_shares_evaluator        >();
  registry.register_evaluator< recurrent_transfer_evaluator             >();
}

void escrow_transfer_evaluator::do_apply( const escrow_transfer_operation& o )
{
  try
  {
    const auto& from_account = _db.get_account( o.from );
    const auto& to_account = _db.get_account( o.to );
    const auto& agent_account = _db.get_account( o.agent );

    HIVE_CHAIN_TIME_ASSERT( o.ratification_deadline > _db.head_block_time(), o.ratification_deadline, "The escrow ratification deadline must be after head block time." );
    HIVE_CHAIN_TIME_ASSERT( o.escrow_expiration > _db.head_block_time(), o.escrow_expiration, "The escrow expiration must be after head block time." );
    HIVE_CHAIN_LIMIT_ASSERT( from_account.pending_escrow_transfers < HIVE_MAX_PENDING_TRANSFERS, from_account.pending_escrow_transfers, "Account already has the maximum number of open escrow transfers." );

    HIVE_asset o_hive_amount = o.get_hive_amount();
    HBD_asset o_hbd_amount = o.get_hbd_amount();
    HIVE_asset hive_spent = o_hive_amount;
    HBD_asset hbd_spent = o_hbd_amount;
    HIVE_asset hive_fee;
    HBD_asset hbd_fee;
    if( o.fee.symbol == HIVE_SYMBOL )
    {
      hive_fee = HIVE_asset( o.fee );
      hive_spent += hive_fee;
    }
    else
    {
      hbd_fee = HBD_asset( o.fee );
      hbd_spent += hbd_fee;
    }

    temp_HIVE_balance escrow_hive;
    _db.adjust_balance( from_account, escrow_hive, -hive_spent );
    temp_HBD_balance escrow_hbd;
    _db.adjust_balance( from_account, escrow_hbd, -hbd_spent );

    temp_balance escrow_fee( o.fee.symbol );
    if( o.fee.symbol == HIVE_SYMBOL )
      escrow_fee.transfer_from( escrow_hive, hive_fee );
    else
      escrow_fee.transfer_from( escrow_hbd, hbd_fee );

    _db.create<escrow_object>( from_account, to_account, agent_account,
      std::move( escrow_hive ), std::move( escrow_hbd ), std::move( escrow_fee ),
      o.ratification_deadline, o.escrow_expiration, o.escrow_id );
    _db.modify( from_account, []( account_object& a )
    {
      a.pending_escrow_transfers++;
    } );
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void escrow_approve_evaluator::do_apply( const escrow_approve_operation& o )
{
  try
  {
    const auto& escrow = _db.get_escrow( o.from, o.escrow_id );

    HIVE_CHAIN_STATE_ASSERT( escrow.get_to() == o.to, o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", escrow.get_to()) );
    HIVE_CHAIN_STATE_ASSERT( escrow.get_agent() == o.agent, o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", escrow.get_agent()) );
    HIVE_CHAIN_TIME_ASSERT( escrow.get_ratification_deadline() >= _db.head_block_time(), escrow.get_ratification_deadline(), "The escrow ratification deadline has passed. Escrow can no longer be ratified." );

    bool reject_escrow = !o.approve;

    if( o.who == o.to )
    {
      HIVE_CHAIN_STATE_ASSERT( !escrow.is_to_approved(), o.to, "Account 'to' (${t}) has already approved the escrow.", ("t", o.to) );

      if( !reject_escrow )
      {
        _db.modify( escrow, [&]( escrow_object& esc )
        {
          esc.approve_to();
        } );
      }
    }
    if( o.who == o.agent )
    {
      HIVE_CHAIN_STATE_ASSERT( !escrow.is_agent_approved(), o.agent, "Account 'agent' (${a}) has already approved the escrow.", ("a", o.agent) );

      if( !reject_escrow )
      {
        _db.modify( escrow, [&]( escrow_object& esc )
        {
          esc.approve_agent();
        } );
      }
    }

    if( reject_escrow )
    {
      _db.remove_escrow( escrow );
    }
    else if( escrow.is_to_approved() && escrow.is_agent_approved() )
    {
      temp_balance fee_released( escrow.get_fee().symbol );
      _db.modify( escrow, [&]( escrow_object& esc )
      {
        esc.access_fee().transfer_to( fee_released );
      } );

      auto vop = escrow_approved_operation( o.from, o.to, o.agent, o.escrow_id, fee_released.as_asset() );
      _db.adjust_balance( o.agent, fee_released, fee_released.as_asset() );
      push_virtual_operation( _db, vop );
    }
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void escrow_dispute_evaluator::do_apply( const escrow_dispute_operation& o )
{
  try
  {
    _db.get_account( o.from ); // Verify from account exists

    const auto& e = _db.get_escrow( o.from, o.escrow_id );
    HIVE_CHAIN_TIME_ASSERT( _db.head_block_time() < e.get_escrow_expiration(), e.get_escrow_expiration(), "Disputing the escrow must happen before expiration." );
    HIVE_CHAIN_STATE_ASSERT( e.is_approved() && "The escrow must be approved by all parties before a dispute can be raised.", o.from, "Escrow for '${subject}' is not fully approved." );
    HIVE_CHAIN_STATE_ASSERT( !e.is_disputed(), o.from, "The escrow is already under dispute." );
    HIVE_CHAIN_STATE_ASSERT( e.get_to() == o.to && "dispute", o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", e.get_to()) );
    HIVE_CHAIN_STATE_ASSERT( e.get_agent() == o.agent && "dispute", o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", e.get_agent()) );

    _db.modify( e, [&]( escrow_object& esc )
    {
      esc.start_dispute();
    } );
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void escrow_release_evaluator::do_apply( const escrow_release_operation& o )
{
  try
  {
    const auto& from_account = _db.get_account( o.from );

    const auto& e = _db.get_escrow( o.from, o.escrow_id );
    HIVE_asset o_hive_amount = o.get_hive_amount();
    HBD_asset o_hbd_amount = o.get_hbd_amount();

    HIVE_CHAIN_BALANCE_ASSERT( e.get_hive_balance() >= o_hive_amount, o.from, "Release amount exceeds escrow balance. Amount: ${a}, Balance: ${b}", ("a", o_hive_amount)("b", e.get_hive_balance()) );
    HIVE_CHAIN_BALANCE_ASSERT( e.get_hbd_balance() >= o_hbd_amount, o.from, "Release amount exceeds escrow balance. Amount: ${a}, Balance: ${b}", ("a", o_hbd_amount)("b", e.get_hbd_balance()) );
    HIVE_CHAIN_STATE_ASSERT( e.get_to() == o.to && "release", o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", e.get_to()) );
    HIVE_CHAIN_STATE_ASSERT( e.get_agent() == o.agent && "release", o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", e.get_agent()) );
    HIVE_CHAIN_PERMISSION_ASSERT( o.receiver == e.get_from() || o.receiver == e.get_to(), o.receiver, "Funds must be released to 'from' (${f}) or 'to' (${t})", ("f", e.get_from())("t", e.get_to()) );
    HIVE_CHAIN_STATE_ASSERT( e.is_approved() && "Funds cannot be released prior to escrow approval.", o.from, "Escrow for '${subject}' is not approved." );

    // If there is a dispute regardless of expiration, the agent can release funds to either party
    if( e.is_disputed() )
    {
      HIVE_CHAIN_PERMISSION_ASSERT( o.who == e.get_agent(), o.who, "Only 'agent' (${a}) can release funds in a disputed escrow.", ("a", e.get_agent()) );
    }
    else
    {
      HIVE_CHAIN_PERMISSION_ASSERT( o.who == e.get_from() || o.who == e.get_to(), o.who, "Only 'from' (${f}) and 'to' (${t}) can release funds from a non-disputed escrow", ("f", e.get_from())("t", e.get_to()) );

      if( e.get_escrow_expiration() > _db.head_block_time() )
      {
        // If there is no dispute and escrow has not expired, either party can release funds to the other.
        if( o.who == e.get_from() )
        {
          HIVE_CHAIN_PERMISSION_ASSERT( o.receiver == e.get_to(), o.who, "Only 'from' (${f}) can release funds to 'to' (${t}).", ("f", e.get_from())("t", e.get_to()) );
        }
        else if( o.who == e.get_to() )
        {
          HIVE_CHAIN_PERMISSION_ASSERT( o.receiver == e.get_from(), o.who, "Only 'to' (${t}) can release funds to 'from' (${t}).", ("f", e.get_from())("t", e.get_to()) );
        }
      }
    }
    // If escrow expires and there is no dispute, either party can release funds to either party.

    temp_HIVE_balance hive_released;
    temp_HBD_balance hbd_released;
    _db.modify( e, [&]( escrow_object& esc )
    {
      esc.access_hive_balance().transfer_to( hive_released, o_hive_amount );
      esc.access_hbd_balance().transfer_to( hbd_released, o_hbd_amount );
    } );

    _db.adjust_balance( o.receiver, hive_released, hive_released.as_asset() );
    _db.adjust_balance( o.receiver, hbd_released, hbd_released.as_asset() );

    if( e.get_hive_balance().amount == 0 && e.get_hbd_balance().amount == 0 )
    {
      _db.modify( from_account, []( account_object& a )
      {
        a.pending_escrow_transfers--;
      } );
      _db.remove( e );
    }
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void transfer_evaluator::do_apply( const transfer_operation& o )
{
  if( _db.has_hardfork( HIVE_HARDFORK_1_24 ) && o.amount.symbol == HIVE_SYMBOL && _db.is_treasury( o.to ) )
  {
    const auto& exchange_rate = _db.get_hbd_price();

    validate_price_feed_available( exchange_rate, "send HIVE to treasury" );

    HIVE_asset o_amount( o.amount );
    auto amount_to_transfer = o_amount * exchange_rate;

    temp_HIVE_balance hive_balance;
    _db.adjust_balance( o.from, hive_balance, -o_amount );
    temp_HBD_balance converted_hbd_balance;
    _db.modify( _db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& dgpo )
    {
      converted_hbd_balance = dgpo.convert_HIVE_to_HBD( hive_balance, exchange_rate, amount_to_transfer.amount > 0 );
    } );
    _db.adjust_balance( o.to, converted_hbd_balance, converted_hbd_balance.as_asset() );

    // o.to will always be the treasury so no need to call _db.get_treasury
    push_virtual_operation( _db, dhf_conversion_operation( o.to, o_amount, amount_to_transfer ) );
    return;
  }
  else if( _db.has_hardfork( HIVE_HARDFORK_0_21__3343 ) )
  {
    HIVE_CHAIN_TREASURY_ASSERT( o.amount.symbol == HBD_SYMBOL || !_db.is_treasury( o.to ), o.to, "Can only transfer HBD or HIVE to ${s}", ( "s", o.to ) );
  }

  temp_balance transfer( o.amount.symbol );
  _db.adjust_balance( o.from, transfer, -o.amount );
  _db.adjust_balance( o.to, transfer, o.amount );
}

void transfer_to_vesting_evaluator::do_apply( const transfer_to_vesting_operation& o )
{
  const auto& from_account = _db.get_account(o.from);
  const auto& to_account = o.to.size() ? _db.get_account(o.to) : from_account;

  if( _db.has_hardfork( HIVE_HARDFORK_0_21__3343 ) )
    HIVE_CHAIN_TREASURY_ASSERT( !_db.is_treasury( o.to ), o.to, "Cannot vest on behalf of ${s}", ("s", o.to ) );

  HIVE_asset o_amount( o.get_amount() );
  temp_HIVE_balance vesting_funds;
  _db.adjust_balance( from_account, vesting_funds, -o_amount );

  VEST_asset amount_vested;

  /*
    Voting immediately when `transfer_to_vesting` is executed is dangerous,
    because malicious accounts would take over whole HIVE network.
    Therefore an idea is based on voting deferring. Default value is 30 days.
    This range of time is enough long to defeat/block potential malicious intention.
  */
  if( _db.has_hardfork( HIVE_HARDFORK_1_24 ) )
  {
    amount_vested = _db.adjust_account_vesting_balance( to_account, vesting_funds, false, []( VEST_asset vests_created ) {} );

    delayed_voting dv( _db );
    dv.add_delayed_value( to_account, _db.head_block_time(), amount_vested.get_amount() );
  }
  else
  {
    amount_vested = _db.create_vesting( to_account, vesting_funds );
  }

  /// Emit this vop unconditionally, since VESTS balance changed immediately, indepdenent to subsequent updates of account voting power done inside `delayed_voting` mechanism.
  push_virtual_operation( _db, transfer_to_vesting_completed_operation( from_account.get_name(), to_account.get_name(), o_amount, amount_vested ) );
}

void withdraw_vesting_evaluator::do_apply( const withdraw_vesting_operation& o )
{
  const auto& account = _db.get_account( o.account );
  auto now = _db.head_block_time();

  VEST_asset o_vesting_shares = o.get_vesting_shares();

  if( o_vesting_shares.amount < 0 )
  {
    HIVE_CHAIN_HARDFORK_ASSERT( !_db.has_hardfork( HIVE_HARDFORK_0_20 ), "Cannot withdraw negative VESTS. account: ${account}, vests:${vests}",
      ( "account", o.account )( "vests", o_vesting_shares ) );
    //see https://peakd.com/imfamy/@ironshield/block-23847548-the-block-which-will-live-in-infamy
    return;
  }

  HIVE_CHAIN_BALANCE_ASSERT( account.get_vesting() >= VEST_asset( 0 ), o.account, "Account does not have sufficient Hive Power for withdraw." );
  HIVE_CHAIN_BALANCE_ASSERT( account.get_vesting() - account.get_delegated_vesting() >= o_vesting_shares, o.account, "Account does not have sufficient Hive Power for withdraw." );

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    _db.rc().regenerate_rc_mana( account, now );
  if( o_vesting_shares.amount == 0 )
  {
    if( _db.has_hardfork( HIVE_HARDFORK_1_28_FIX_CANCEL_POWER_DOWN ) )
      HIVE_CHAIN_STATE_ASSERT( account.has_active_power_down(), o.account, "This operation would not change the vesting withdraw rate." );

    _db.modify( account, [&]( account_object& a )
    {
      a.vesting_withdraw_rate = VEST_asset( 0 );
      a.next_vesting_withdrawal = time_point_sec::maximum();
      a.to_withdraw = VEST_asset( 0 );
      a.withdrawn = VEST_asset( 0 );
    } );
  }
  else
  {
    int vesting_withdraw_intervals = HIVE_VESTING_WITHDRAW_INTERVALS_PRE_HF_16;
    if( _db.has_hardfork( HIVE_HARDFORK_0_16__551 ) )
      vesting_withdraw_intervals = HIVE_VESTING_WITHDRAW_INTERVALS; /// 13 weeks = 1 quarter of a year

    _db.modify( account, [&]( account_object& a )
    {
      VEST_asset new_vesting_withdraw_rate = o_vesting_shares / vesting_withdraw_intervals;

      if( new_vesting_withdraw_rate.amount == 0 )
        new_vesting_withdraw_rate.amount = 1;

      if( _db.has_hardfork( HIVE_HARDFORK_0_21 ) && new_vesting_withdraw_rate.amount * vesting_withdraw_intervals < o_vesting_shares.amount )
      {
        new_vesting_withdraw_rate.amount += 1;
      }

      if( _db.has_hardfork( HIVE_HARDFORK_1_28_FIX_CANCEL_POWER_DOWN ) )
        HIVE_CHAIN_STATE_ASSERT( account.vesting_withdraw_rate != new_vesting_withdraw_rate, o.account, "This operation would not change the vesting withdraw rate." );

      a.vesting_withdraw_rate = new_vesting_withdraw_rate;
      a.next_vesting_withdrawal = now + fc::seconds( HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS );
      a.to_withdraw = o_vesting_shares;
      a.withdrawn = VEST_asset( 0 );
    } );
  }
  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    _db.rc().update_account_after_vest_change( account, now, false, true );
}

void set_withdraw_vesting_route_evaluator::do_apply( const set_withdraw_vesting_route_operation& o )
{
  try
  {
  const auto& from_account = _db.get_account( o.from_account );
  const auto& to_account = _db.get_account( o.to_account );
  const auto& wd_idx = _db.get_index< withdraw_vesting_route_index >().indices().get< by_withdraw_route >();
  auto itr = wd_idx.find( boost::make_tuple( from_account.get_name(), to_account.get_name() ) );

  if( _db.has_hardfork( HIVE_HARDFORK_0_21__3343 ) )
  {
    HIVE_CHAIN_TREASURY_ASSERT( !_db.is_treasury( o.to_account ), o.to_account, "Cannot withdraw vesting to ${s}", ("s", o.to_account ) );
  }

  if( itr == wd_idx.end() )
  {
    HIVE_CHAIN_STATE_ASSERT( o.percent != 0, o.from_account, "Cannot create a 0% destination." );
    HIVE_CHAIN_LIMIT_ASSERT( from_account.withdraw_routes < HIVE_MAX_WITHDRAW_ROUTES, from_account.withdraw_routes, "Account already has the maximum number of routes." );

    _db.create< withdraw_vesting_route_object >( [&]( withdraw_vesting_route_object& wvdo )
    {
      wvdo.from_account = from_account.get_name();
      wvdo.to_account = to_account.get_name();
      wvdo.percent = o.percent;
      wvdo.auto_vest = o.auto_vest;
    });

    _db.modify( from_account, [&]( account_object& a )
    {
      a.withdraw_routes++;
    });
  }
  else if( o.percent == 0 )
  {
    _db.remove( *itr );

    _db.modify( from_account, [&]( account_object& a )
    {
      a.withdraw_routes--;
    });
  }
  else
  {
    _db.modify( *itr, [&]( withdraw_vesting_route_object& wvdo )
    {
      wvdo.from_account = from_account.get_name();
      wvdo.to_account = to_account.get_name();
      wvdo.percent = o.percent;
      wvdo.auto_vest = o.auto_vest;
    });
  }

  itr = wd_idx.upper_bound( boost::make_tuple( from_account.get_name(), account_name_type() ) );
  uint16_t total_percent = 0;

  while( itr != wd_idx.end() && itr->from_account == from_account.get_name() )
  {
    total_percent += itr->percent;
    ++itr;
  }

  HIVE_CHAIN_LIMIT_ASSERT( total_percent <= HIVE_100_PERCENT, total_percent, "More than 100% of vesting withdrawals allocated to destinations." );
  }
  FC_CAPTURE_AND_RETHROW()
}

void convert_evaluator::do_apply( const convert_operation& o )
{
  const auto& owner = _db.get_account( o.owner );
  HBD_asset o_amount = o.get_amount();
  temp_HBD_balance convert_amount;
  _db.adjust_balance( owner, convert_amount, -o_amount );

  const auto& exchange_rate = _db.get_hbd_price();
  validate_price_feed_available( exchange_rate, "convert HBD" );

  auto hive_conversion_delay = HIVE_CONVERSION_DELAY_PRE_HF_16;
  if( _db.has_hardfork( HIVE_HARDFORK_0_16__551) )
    hive_conversion_delay = HIVE_CONVERSION_DELAY;

  _db.create<convert_request_object>( owner, std::move( convert_amount ),
    _db.head_block_time() + hive_conversion_delay, o.requestid );
}

void collateralized_convert_evaluator::do_apply( const collateralized_convert_operation& o )
{
  HIVE_CHAIN_HARDFORK_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_25 ) && "Operation not available until HF25", "" );

  const auto& owner = _db.get_account( o.owner );
  HIVE_asset o_amount = o.get_amount();
  temp_HIVE_balance collateral;
  _db.adjust_balance( owner, collateral, -o_amount );

  const auto& fhistory = _db.get_feed_history();
  validate_price_feed_available( fhistory.current_median_history, "convert HIVE" );
  const auto& dgpo = _db.get_dynamic_global_properties();
  HIVE_CHAIN_STATE_ASSERT( dgpo.get_hbd_print_rate() > 0, dgpo.get_hbd_print_rate(), "Creation of new HBD blocked at this time due to global limit." );
    //note that feed and therefore print rate is updated once per hour, so without above check there could be
    //enough room for new HBD, but there is a chance the official price would still be artificial (artificial
    //price is not used in this conversion, but users might think it is - better to stop them from making mistake)

  //if you change something here take a look at wallet_api::estimate_hive_collateral as well

  //cut amount by collateral ratio
  uint128_t _amount = ( uint128_t( o_amount.amount.value ) * HIVE_100_PERCENT ) / HIVE_CONVERSION_COLLATERAL_RATIO;
  HIVE_asset for_immediate_conversion = HIVE_asset( fc::uint128_to_uint64( _amount ) );

  //immediately create HBD - apply fee to current rolling minimum price
  HBD_asset converted_amount = multiply_with_fee< HIVE_ASSET_NUM_HIVE >( for_immediate_conversion,
    fhistory.current_min_history, HIVE_COLLATERALIZED_CONVERSION_FEE );
  HIVE_CHAIN_ASSET_ASSERT( converted_amount.amount > 0, converted_amount, "Amount of collateral too low - conversion gives no HBD" );

  temp_HBD_balance converted;
  _db.modify( dgpo, [&]( dynamic_global_property_object& p )
  {
    //HIVE supply (and virtual supply in part related to HIVE) will be corrected after actual conversion
    converted = p.issue_HBD( converted_amount, fhistory.current_median_history );
  } );
  //note that we created new HBD out of thin air and we will burn the related HIVE when actual conversion takes
  //place; there is alternative approach - we could burn all collateral now and print back excess when we are
  //to return it to the user; the difference slightly changes calculations below, that is, currently we might
  //allow slightly more HBD to be printed
  _db.adjust_balance( owner, converted, converted.as_asset() );

  uint16_t percent_hbd = _db.calculate_HBD_percent();
  HIVE_CHAIN_LIMIT_ASSERT( percent_hbd <= dgpo.get_hbd_stop_percent(), percent_hbd, "Creation of new ${hbd} violates global limit.", ( "hbd", converted_amount ) );

  _db.create<collateralized_convert_request_object>( owner, std::move( collateral ), converted_amount,
    _db.head_block_time() + HIVE_COLLATERALIZED_CONVERSION_DELAY, o.requestid );

  push_virtual_operation( _db, collateralized_convert_immediate_conversion_operation( o.owner, o.requestid, converted_amount ) );
}

void limit_order_create_evaluator::do_apply( const limit_order_create_operation& o )
{
  HIVE_CHAIN_TIME_ASSERT( o.expiration > _db.head_block_time() && "Limit order", o.expiration,
    "Limit order has to expire after head block time." );

  // once HF29 triggers try trimming the check in validate
  if( _db.is_in_control() || _db.has_hardfork( HIVE_HARDFORK_1_29_FIX_LIMIT_ORDER_ASSETS ) )
  {
    HIVE_PROTOCOL_ASSET_ASSERT( ( is_asset_type( o.amount_to_sell, HIVE_SYMBOL ) && is_asset_type( o.min_to_receive, HBD_SYMBOL ) )
      || ( is_asset_type( o.amount_to_sell, HBD_SYMBOL ) && is_asset_type( o.min_to_receive, HIVE_SYMBOL ) ),
      "Limit order must be for the HIVE:HBD market", ("subject", o.amount_to_sell)(o.min_to_receive) );
  }

  time_point_sec expiration = o.expiration;
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1449 ) ) // 307842c657d54f576615cd312a425c517ad68db4 example tx with extra long expiration
  {
    HIVE_CHAIN_TIME_ASSERT( o.expiration <= _db.head_block_time() + HIVE_MAX_LIMIT_ORDER_EXPIRATION, o.expiration, "Limit Order Expiration must not be more than 28 days in the future" );
  }
#ifndef HIVE_CONVERTER_BUILD // limit_order_object expiration time is explicitly set during the conversion time before HF20, due to the altered block id
  else
  {
    uint32_t rand_offset = _db.head_block_id()._hash[ 4 ] % 86400;
    expiration = std::min( o.expiration, fc::time_point_sec( HIVE_HARDFORK_0_20_TIME + HIVE_MAX_LIMIT_ORDER_EXPIRATION + rand_offset ) );
  }
#endif

  const auto& seller = _db.get_account( o.owner );
  temp_balance amount_to_sell( o.amount_to_sell.symbol );
  _db.adjust_balance( seller, amount_to_sell, -o.amount_to_sell );

  const auto& order = _db.create<limit_order_object>( seller, std::move( amount_to_sell ),
    o.get_price(), _db.head_block_time(), expiration, o.orderid );

  bool filled = _db.apply_order( order );

  HIVE_CHAIN_STATE_ASSERT( not o.fill_or_kill || filled, o.owner, "Cancelling order because it was not filled." );
}

void limit_order_create2_evaluator::do_apply( const limit_order_create2_operation& o )
{
  HIVE_CHAIN_TIME_ASSERT( o.expiration > _db.head_block_time() && "Limit order 2",
    o.expiration, "Limit order has to expire after head block time." );

  // once HF29 triggers try trimming the check in validate
  if( _db.is_in_control() || _db.has_hardfork( HIVE_HARDFORK_1_29_FIX_LIMIT_ORDER_ASSETS ) )
  {
    HIVE_PROTOCOL_ASSET_ASSERT( ( is_asset_type( o.amount_to_sell, HIVE_SYMBOL ) && is_asset_type( o.exchange_rate.quote, HBD_SYMBOL ) )
      || ( is_asset_type( o.amount_to_sell, HBD_SYMBOL ) && is_asset_type( o.exchange_rate.quote, HIVE_SYMBOL ) ),
      "Limit order must be for the HIVE:HBD market", ("subject", o.amount_to_sell)(o.exchange_rate) );
  }

  time_point_sec expiration = o.expiration;
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1449 ) ) // 8bce7ca4b2bea9af848db927342a88f82eea0684 example tx with extra long expiration
  {
    HIVE_CHAIN_TIME_ASSERT( o.expiration <= _db.head_block_time() + HIVE_MAX_LIMIT_ORDER_EXPIRATION && "Limit Order Expiration must not be more than 28 days in the future", o.expiration, "Order expiration ${subject} exceeds 28-day limit." );
  }
  else
  {
    expiration = std::min( o.expiration, fc::time_point_sec( HIVE_HARDFORK_0_20_TIME + HIVE_MAX_LIMIT_ORDER_EXPIRATION ) );
  }

  const auto& seller = _db.get_account( o.owner );
  temp_balance amount_to_sell( o.amount_to_sell.symbol );
  _db.adjust_balance( seller, amount_to_sell, -o.amount_to_sell );

  const auto& order = _db.create<limit_order_object>( seller, std::move( amount_to_sell ),
    o.exchange_rate, _db.head_block_time(), expiration, o.orderid );

  bool filled = _db.apply_order( order );

  if( o.fill_or_kill ) HIVE_CHAIN_STATE_ASSERT( filled && "Cancelling order because it was not filled.", o.owner, "Fill-or-kill order for '${subject}' was not filled." );
}

void limit_order_cancel_evaluator::do_apply( const limit_order_cancel_operation& o )
{
  _db.cancel_order( _db.get_limit_order( o.owner, o.orderid ) );
}

void transfer_to_savings_evaluator::do_apply( const transfer_to_savings_operation& op )
{
  const auto& from = _db.get_account( op.from );
  const auto& to   = _db.get_account(op.to);

  HIVE_CHAIN_TREASURY_ASSERT( !_db.is_treasury( op.to ), op.to, "Cannot transfer to savings of treasury ${s}", ("s", op.to ) );

  temp_balance transfer( op.amount.symbol );
  _db.adjust_balance( from, transfer, -op.amount );
  _db.adjust_savings_balance( to, transfer, op.amount );
}

void transfer_from_savings_evaluator::do_apply( const transfer_from_savings_operation& op )
{
  const auto& from = _db.get_account( op.from );
  const auto& to   = _db.get_account( op.to );

  HIVE_CHAIN_LIMIT_ASSERT( from.savings_withdraw_requests < HIVE_SAVINGS_WITHDRAW_REQUEST_LIMIT, from.savings_withdraw_requests, "Account has reached limit for pending withdraw requests." );

  HIVE_CHAIN_TREASURY_ASSERT( op.amount.symbol == HBD_SYMBOL || !_db.is_treasury( op.to ), op.to, "Can only transfer HBD to ${s}", ("s", op.to ) );

  HIVE_CHAIN_BALANCE_ASSERT( _db.get_savings_balance( from, op.amount.symbol ) >= op.amount, op.from, "Insufficient savings balance for '${subject}'." );
  temp_balance withdraw_amount( op.amount.symbol );
  _db.adjust_savings_balance( from, withdraw_amount, -op.amount );

  _db.create<savings_withdraw_object>( from, to, std::move( withdraw_amount ),
    op.memo, _db.head_block_time() + HIVE_SAVINGS_WITHDRAW_TIME, op.request_id );

  _db.modify( from, [&]( account_object& a )
  {
    a.savings_withdraw_requests++;
  } );
}

void cancel_transfer_from_savings_evaluator::do_apply( const cancel_transfer_from_savings_operation& op )
{
  const auto& swo = _db.get_savings_withdraw( op.from, op.request_id );

  temp_balance returned( swo.get_withdraw_amount().symbol );
  _db.modify( swo, [&]( savings_withdraw_object& s )
  {
    s.access_amount().transfer_to( returned );
  } );

  _db.adjust_savings_balance( _db.get_account( swo.get_from() ), returned, returned.as_asset() );
  _db.remove( swo );

  const auto& from = _db.get_account( op.from );
  _db.modify( from, [&]( account_object& a )
  {
    a.savings_withdraw_requests--;
  } );
}

void claim_reward_balance_evaluator::do_apply( const claim_reward_balance_operation& op )
{
  const auto& acnt = _db.get_account( op.account );
  const auto& dgpo = _db.get_dynamic_global_properties();
  auto now = dgpo.get_head_block_time();

  HIVE_asset op_reward_hive = op.get_reward_hive();
  HBD_asset op_reward_hbd = op.get_reward_hbd();
  VEST_asset op_reward_vests = op.get_reward_vests();

  HIVE_CHAIN_BALANCE_ASSERT( op_reward_hive <= acnt.get_hive_rewards(), op.account, "Cannot claim that much HIVE. Claim: ${c} Actual: ${a}",
    ( "c", op_reward_hive )( "a", acnt.get_hive_rewards() ) );
  HIVE_CHAIN_BALANCE_ASSERT( op_reward_hbd <= acnt.get_hbd_rewards(), op.account, "Cannot claim that much HBD. Claim: ${c} Actual: ${a}",
    ( "c", op_reward_hbd )( "a", acnt.get_hbd_rewards() ) );
  HIVE_CHAIN_BALANCE_ASSERT( op_reward_vests <= acnt.get_vest_rewards(), op.account, "Cannot claim that much VESTS. Claim: ${c} Actual: ${a}",
    ( "c", op_reward_vests )( "a", acnt.get_vest_rewards() ) );

  HIVE_asset reward_vesting_hive_to_move = HIVE_asset( 0 );
  if( op_reward_vests == acnt.get_vest_rewards() )
    reward_vesting_hive_to_move = acnt.get_vest_rewards_as_hive();
  else
    reward_vesting_hive_to_move = HIVE_asset( fc::uint128_to_uint64( ( uint128_t( op_reward_vests.amount.value ) * uint128_t( acnt.get_vest_rewards_as_hive().amount.value ) )
      / uint128_t( acnt.get_vest_rewards().amount.value ) ) );

  temp_HIVE_balance hive_reward;
  _db.adjust_reward_balance( acnt, hive_reward, -op_reward_hive );
  _db.adjust_balance( acnt, hive_reward, op_reward_hive );
  temp_HBD_balance hbd_reward;
  _db.adjust_reward_balance( acnt, hbd_reward, -op_reward_hbd );
  _db.adjust_balance( acnt, hbd_reward, op_reward_hbd );

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    _db.rc().regenerate_rc_mana( acnt, now );
  temp_HIVE_balance reward_vesting_hive_balance;
  _db.modify( acnt, [&]( account_object& a )
  {
    if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
      util::update_manabar( dgpo, a, op_reward_vests.amount.value );
    a.access_vesting().transfer_from( a.access_vest_rewards(), op_reward_vests );
    reward_vesting_hive_balance.transfer_from( a.access_vest_rewards_as_hive(), reward_vesting_hive_to_move );
  } );
  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    _db.rc().update_account_after_vest_change( acnt, now );

  _db.modify( dgpo, [&]( dynamic_global_property_object& gpo )
  {
    gpo.access_total_vesting_shares() += op_reward_vests;
    gpo.access_total_vesting_fund_hive().transfer_from( reward_vesting_hive_balance );

    gpo.access_pending_rewarded_vesting_shares() -= op_reward_vests;
    gpo.access_pending_rewarded_vesting_hive() -= reward_vesting_hive_to_move;
  } );

  _db.adjust_proxied_witness_votes( acnt, op_reward_vests.amount );
}

void delegate_vesting_shares_evaluator::do_apply( const delegate_vesting_shares_operation& op )
{
  const auto& delegator = _db.get_account( op.delegator );
  const auto& delegatee = _db.get_account( op.delegatee );
  auto* delegation = _db.find< vesting_delegation_object, by_delegation >( boost::make_tuple( delegator.get_id(), delegatee.get_id() ) );

  const auto& gpo = _db.get_dynamic_global_properties();
  auto now = gpo.get_head_block_time();

  VEST_asset op_vesting_shares = op.get_vesting_shares();
  VEST_asset available_shares;
  VEST_asset available_downvote_shares;

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
  {
    _db.rc().regenerate_rc_mana( delegator, now );
    _db.rc().regenerate_rc_mana( delegatee, now );
  }

  if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
  {
    auto max_mana = delegator.get_effective_vesting_shares().amount;

    _db.modify( delegator, [&]( account_object& a )
    {
      util::update_manabar( gpo, a );
    } );

    available_shares = VEST_asset( delegator.voting_manabar.current_mana );
    if( gpo.get_downvote_pool_percent() )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
      {
        available_downvote_shares = VEST_asset(
          fc::uint128_to_int64( ( uint128_t( delegator.downvote_manabar.current_mana ) * HIVE_100_PERCENT ) / gpo.get_downvote_pool_percent()
          + ( HIVE_100_PERCENT / gpo.get_downvote_pool_percent() ) - 1 ) );
      }
      else
      {
        available_downvote_shares = VEST_asset(
          ( delegator.downvote_manabar.current_mana * HIVE_100_PERCENT ) / gpo.get_downvote_pool_percent()
          + ( HIVE_100_PERCENT / gpo.get_downvote_pool_percent() ) - 1 );
      }
    }
    else
    {
      available_downvote_shares = available_shares;
    }

    // Assume delegated VESTS are used first when consuming mana. You cannot delegate received vesting shares
    available_shares.amount = std::min( available_shares.amount, max_mana - delegator.get_received_vesting().amount );
    available_downvote_shares.amount = std::min( available_downvote_shares.amount, max_mana - delegator.get_received_vesting().amount );

    if( delegator.next_vesting_withdrawal < fc::time_point_sec::maximum()
      && delegator.get_total_vesting_withdrawal() > delegator.vesting_withdraw_rate )
    {
      /*
      Account cannot delegate **any** VESTS that they are powering down. Therefore we have to reduce
      available shares by whole remaining power down. However current voting mana does not include current
      week's power down, so we have to add it, otherwise it would be effectively subtracted twice. We can
      skip that step when power down is in last week, because then whole power down (subtracting) equals
      current week's power down (adding).
      */
      auto weekly_withdraw = delegator.get_next_vesting_withdrawal();
      auto remaining_withdraw = delegator.get_total_vesting_withdrawal();
      available_shares += weekly_withdraw - remaining_withdraw;
      available_downvote_shares += weekly_withdraw - remaining_withdraw;
    }
  }
  else
  {
    available_shares = delegator.get_vesting() - delegator.get_delegated_vesting() - delegator.get_total_vesting_withdrawal();
    available_downvote_shares = available_shares;
  }

  // HF 20 increase fee meaning by 30x, reduce these thresholds to compensate.
  HIVE_asset fee = _db.get_witness_schedule_object().median_props.account_creation_fee;
  VEST_asset min_delegation = _db.has_hardfork( HIVE_HARDFORK_0_20__1761 ) ?
    ( fee / 3 ) * gpo.get_vesting_share_price() : ( fee * 10 ) * gpo.get_vesting_share_price();
  VEST_asset min_update = _db.has_hardfork( HIVE_HARDFORK_0_20__1761 ) ?
    ( fee / 30 ) * gpo.get_vesting_share_price() : fee * gpo.get_vesting_share_price();

  if( delegation == nullptr ) // delegation doesn't exist, create it
  {
    HIVE_CHAIN_BALANCE_ASSERT( available_shares >= op_vesting_shares, op.delegator, "Account ${acc} does not have enough mana to delegate. required: ${r} available: ${a}",
      ( "acc", op.delegator )( "r", op_vesting_shares )( "a", available_shares ) );
    HIVE_CHAIN_BALANCE_ASSERT( available_downvote_shares >= op_vesting_shares, op.delegator, "Account ${acc} does not have enough downvote mana to delegate. required: ${r} available: ${a}",
      ( "acc", op.delegator )( "r", op_vesting_shares )( "a", available_downvote_shares ) );
    HIVE_CHAIN_LIMIT_ASSERT( op_vesting_shares >= min_delegation && "Minimum not reached", op_vesting_shares, "Account must delegate a minimum of ${v}", ( "v", min_delegation ) );

    _db.create< vesting_delegation_object >( delegator, delegatee, op_vesting_shares, now );

    _db.modify( delegator, [&]( account_object& a )
    {
      a.access_delegated_vesting() += op_vesting_shares;

      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        a.voting_manabar.use_mana( op_vesting_shares.amount.value );

        if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
        {
          a.downvote_manabar.use_mana( fc::uint128_to_int64( ( uint128_t( op_vesting_shares.amount.value ) * gpo.get_downvote_pool_percent() ) / HIVE_100_PERCENT ) );
        }
        else if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) )
        {
          a.downvote_manabar.use_mana( op_vesting_shares.amount.value );
        }
      }
    } );

    _db.modify( delegatee, [&]( account_object& a )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        util::update_manabar( gpo, a, op_vesting_shares.amount.value );
      }

      a.access_received_vesting() += op_vesting_shares;
    } );
  }
  else if( op_vesting_shares >= delegation->get_vesting() ) // delegation is increasing
  {
    auto delta = op_vesting_shares - delegation->get_vesting();

    HIVE_CHAIN_LIMIT_ASSERT( delta >= min_update && "Wrong increase", delta, "Hive Power increase is not enough of a difference. min_update: ${min}", ("min", min_update) );
    HIVE_CHAIN_BALANCE_ASSERT( available_shares >= delta, op.delegator, "Account ${acc} does not have enough mana to delegate. required: ${r} available: ${a}",
      ( "acc", op.delegator )( "r", delta )( "a", available_shares ) );
    HIVE_CHAIN_BALANCE_ASSERT( available_downvote_shares >= delta, op.delegator, "Account ${acc} does not have enough downvote mana to delegate. required: ${r} available: ${a}",
      ( "acc", op.delegator )( "r", delta )( "a", available_downvote_shares ) );

    _db.modify( delegator, [&]( account_object& a )
    {
      a.access_delegated_vesting() += delta;

      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        a.voting_manabar.use_mana( delta.amount.value );

        if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
        {
          a.downvote_manabar.use_mana( fc::uint128_to_int64( ( uint128_t( delta.amount.value ) * gpo.get_downvote_pool_percent() ) / HIVE_100_PERCENT ) );
        }
        else if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) )
        {
          a.downvote_manabar.use_mana( delta.amount.value );
        }
      }
    } );

    _db.modify( delegatee, [&]( account_object& a )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        util::update_manabar( gpo, a, delta.amount.value );
      }

      a.access_received_vesting() += delta;
    } );

    _db.modify( *delegation, [&]( vesting_delegation_object& obj )
    {
      obj.set_vesting( op_vesting_shares );
    } );
  }
  else // delegation is decreasing
  {
    auto delta = delegation->get_vesting() - op_vesting_shares;

    if( op_vesting_shares.amount > 0 )
    {
      HIVE_CHAIN_LIMIT_ASSERT( delta >= min_update && "Wrong decrease", delta, "Hive Power decrease is not enough of a difference. min_update: ${min}", ( "min", min_update ) );
      HIVE_CHAIN_LIMIT_ASSERT( op_vesting_shares >= min_delegation, op_vesting_shares, "Delegation must be removed or leave minimum delegation amount of ${v}", ( "v", min_delegation ) );
    }
    else
    {
      HIVE_CHAIN_STATE_ASSERT( delegation->get_vesting().amount > 0, op.delegator, "Delegation would set vesting_shares to zero, but it is already zero" );
    }

    _db.create< vesting_delegation_expiration_object >( delegator, delta,
      std::max( now + gpo.get_delegation_return_period(), delegation->get_min_delegation_time() ) );

    _db.modify( delegatee, [&]( account_object& a )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
      {
        util::update_manabar( gpo, a );
      }

      a.access_received_vesting() -= delta;

      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        a.voting_manabar.use_mana( delta.amount.value );

        if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) )
        {
          if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
          {
            a.downvote_manabar.use_mana( fc::uint128_to_int64( ( uint128_t( delta.amount.value ) * gpo.get_downvote_pool_percent() ) / HIVE_100_PERCENT ) );
          }
          else
          {
            a.downvote_manabar.use_mana( op_vesting_shares.amount.value );
          }
        }
      }
    } );

    if( op_vesting_shares.amount > 0 )
    {
      _db.modify( *delegation, [&]( vesting_delegation_object& obj )
      {
        obj.set_vesting( op_vesting_shares );
      } );
    }
    else
    {
      _db.remove( *delegation );
    }
  }

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
  {
    _db.rc().update_account_after_vest_change( delegator, now, true, true );
    _db.rc().update_account_after_vest_change( delegatee, now, true, true );
  }
}

void recurrent_transfer_evaluator::do_apply( const recurrent_transfer_operation& op )
{
  HIVE_CHAIN_HARDFORK_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_25 ), "Recurrent transfers are not enabled until hardfork ${hf}", ("hf", HIVE_HARDFORK_1_25) );

  const auto& from_account = _db.get_account( op.from );
  const auto& to_account = _db.get_account( op.to );

  asset available = _db.get_balance( from_account, op.amount.symbol );
  HIVE_CHAIN_BALANCE_ASSERT( available >= op.amount, op.from, "Account does not have enough tokens for the first transfer, has ${has} needs ${needs}", ("has", available)("needs", op.amount) );

  uint8_t rtp_id = op.get_pair_id();

  const auto& rt_idx = _db.get_index< recurrent_transfer_index, by_from_to_id >();
  auto itr = rt_idx.find( boost::make_tuple( from_account.get_id(), to_account.get_id(), rtp_id ) );

  if( itr == rt_idx.end() )
  {
    HIVE_CHAIN_LIMIT_ASSERT( from_account.open_recurrent_transfers < HIVE_MAX_OPEN_RECURRENT_TRANSFERS, from_account.open_recurrent_transfers, "Account can't have more than ${rt} recurrent transfers", ( "rt", HIVE_MAX_OPEN_RECURRENT_TRANSFERS ) );
    // If the recurrent transfer is not found and the amount is 0 it means the user wants to delete a transfer that doesn't exist
    HIVE_CHAIN_ASSET_ASSERT( op.amount.amount != 0, op.amount, "Cannot create a recurrent transfer with 0 amount" );
    // A new recurrent transfer always requires at least 2 executions - a single execution would fire
    // immediately and delete itself, in which case a normal transfer should be used instead.
    // NB: the create and modify paths must use textually distinct assertion expressions
    // ('>= 2' here vs '> 1' below) so the assertion-id generator does not produce a hash collision.
    HIVE_CHAIN_LIMIT_ASSERT( op.executions >= 2, op.executions,
      "Executions must be at least 2, if you set executions to 1 the recurrent transfer will execute immediately "
      "and delete itself. You should use a normal transfer operation" );
    const auto& recurrent_transfer = _db.create< recurrent_transfer_object >( _db.head_block_time(), from_account, to_account, op.amount, op.memo, op.recurrence, op.executions, rtp_id );
    HIVE_CHAIN_TIME_ASSERT( recurrent_transfer.get_final_trigger_date() <= _db.head_block_time() + fc::days( HIVE_MAX_RECURRENT_TRANSFER_END_DATE ),
      recurrent_transfer.get_final_trigger_date(), "Cannot set a transfer that would last for longer than ${days} days", ( "days", HIVE_MAX_RECURRENT_TRANSFER_END_DATE ) );

    _db.modify(from_account, [](account_object& a )
    {
      a.open_recurrent_transfers++;
    } );
  }
  else if( op.amount.amount == 0 )
  {
    // Pre-HF29 nodes reject executions == 1 on every path (incl. delete) in validate(); TODO: delete post HF29
    if( !_db.has_hardfork( HIVE_HARDFORK_1_29_ALLOW_MODIFY_LAST_RECURRENT_TRANSFER ) )
    {
      HIVE_CHAIN_LIMIT_ASSERT( 2 <= op.executions, op.executions,
        "Executions must be at least 2 when deleting a recurrent transfer before HF29, "
        "setting executions to 1 was rejected by pre-HF29 nodes" );
    }
    _db.remove( *itr );
    _db.modify( from_account, [&](account_object& a )
    {
      HIVE_CHAIN_STATE_ASSERT( a.open_recurrent_transfers > 0 && "No (more) open recurrent transfers", op.from, "Account '${subject}' has no open recurrent transfers." );
      a.open_recurrent_transfers--;
    } );
  }
  else
  {
    // Modifying an existing recurrent transfer. Before HF29 a modification also required at least 2
    // executions; since HF29 executions may be set to 1 so users can update the last remaining
    // execution (e.g. change the amount or memo on the final payment) - see issue #786.
    // The expression is kept textually different from the create path above ('> 1' vs '>= 2') so the
    // two assertions hash to distinct assertion ids.
    if( !_db.has_hardfork( HIVE_HARDFORK_1_29_ALLOW_MODIFY_LAST_RECURRENT_TRANSFER ) )
    {
      HIVE_CHAIN_LIMIT_ASSERT( op.executions > 1, op.executions,
        "Executions must be at least 2 when modifying a recurrent transfer before HF29, "
        "setting executions to 1 would cause the transfer to execute immediately and delete itself" );
    }
    const auto& recurrent_transfer = *itr;
    _db.modify( recurrent_transfer, [&]( recurrent_transfer_object& rt )
    {
      rt.amount = op.amount;
      from_string( rt.memo, op.memo );
      rt.set_recurrence_trigger_date( _db.head_block_time(), op.recurrence );
      rt.remaining_executions = op.executions;
    } );
    HIVE_CHAIN_TIME_ASSERT( recurrent_transfer.get_final_trigger_date() <= _db.head_block_time() + fc::days( HIVE_MAX_RECURRENT_TRANSFER_END_DATE ) && "too long",
      recurrent_transfer.get_final_trigger_date(), "Cannot set a transfer that would last for longer than ${days} days", ( "days", HIVE_MAX_RECURRENT_TRANSFER_END_DATE ) );
  }
}

} } // hive::chain

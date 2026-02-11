#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/fixed_string.hpp>

#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_virtual_operations.hpp>
#include <hive/chain/rc/rc_utility.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/detail/state/convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/collateralized_convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/feed_history_object.hpp>
#include <hive/chain/detail/state/escrow_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>
#include <hive/chain/detail/state/savings_withdraw_object_multiindex.hpp>
#include <hive/chain/detail/state/withdraw_vesting_route_object_multiindex.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object_multiindex.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/global_property_object_multiindex.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/detail/state/assets_object.hpp>
#include <hive/chain/detail/state/time_object.hpp>
#include <hive/chain/detail/state/manabars_rc_object.hpp>

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
#ifdef HIVE_ENABLE_SMT
HIVE_DEFINE_EVALUATOR( claim_reward_balance2 )
#endif
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
#ifdef HIVE_ENABLE_SMT
  registry.register_evaluator< claim_reward_balance2_evaluator          >();
#endif
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

    FC_ASSERT( o.ratification_deadline > _db.head_block_time(), "The escrow ratification deadline must be after head block time." );
    FC_ASSERT( o.escrow_expiration > _db.head_block_time(), "The escrow expiration must be after head block time." );
    FC_ASSERT( from_account.get_pending_escrow_transfers() < HIVE_MAX_PENDING_TRANSFERS, "Account already has the maximum number of open escrow transfers." );

    HIVE_asset o_hive_amount = o.get_hive_amount();
    HBD_asset o_hbd_amount = o.get_hbd_amount();
    HIVE_asset hive_spent = o_hive_amount;
    HBD_asset hbd_spent = o_hbd_amount;
    if( o.fee.symbol == HIVE_SYMBOL )
      hive_spent += HIVE_asset( o.fee );
    else
      hbd_spent += HBD_asset( o.fee );

    _db.adjust_balance( from_account, -hive_spent );
    _db.adjust_balance( from_account, -hbd_spent );

    _db.create<escrow_object>( from_account, to_account, agent_account, o_hive_amount, o_hbd_amount, o.fee, o.ratification_deadline, o.escrow_expiration, o.escrow_id );
    _db.modify( from_account, []( account_object& a )
    {
      a.set_pending_escrow_transfers( a.get_pending_escrow_transfers() + 1 );
    } );
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void escrow_approve_evaluator::do_apply( const escrow_approve_operation& o )
{
  try
  {

    const auto& escrow = _db.get_escrow( o.from, o.escrow_id );

    FC_ASSERT( escrow.to == o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", escrow.to) );
    FC_ASSERT( escrow.agent == o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", escrow.agent) );
    FC_ASSERT( escrow.ratification_deadline >= _db.head_block_time(), "The escrow ratification deadline has passed. Escrow can no longer be ratified." );

    bool reject_escrow = !o.approve;

    if( o.who == o.to )
    {
      FC_ASSERT( !escrow.to_approved, "Account 'to' (${t}) has already approved the escrow.", ("t", o.to) );

      if( !reject_escrow )
      {
        _db.modify( escrow, [&]( escrow_object& esc )
        {
          esc.to_approved = true;
        });
      }
    }
    if( o.who == o.agent )
    {
      FC_ASSERT( !escrow.agent_approved, "Account 'agent' (${a}) has already approved the escrow.", ("a", o.agent) );

      if( !reject_escrow )
      {
        _db.modify( escrow, [&]( escrow_object& esc )
        {
          esc.agent_approved = true;
        });
      }
    }

    if( reject_escrow )
    {
      _db.adjust_balance( o.from, escrow.get_hive_balance() );
      _db.adjust_balance( o.from, escrow.get_hbd_balance() );
      _db.adjust_balance( o.from, escrow.get_fee() );

      push_virtual_operation( _db, escrow_rejected_operation( o.from, o.to, o.agent, o.escrow_id,
        escrow.get_hbd_balance(), escrow.get_hive_balance(), escrow.get_fee() ) );

      _db.modify( _db.get_account( escrow.from ), []( account_object& a )
      {
        a.set_pending_escrow_transfers( a.get_pending_escrow_transfers() - 1 );
      } );
      _db.remove( escrow );
    }
    else if( escrow.to_approved && escrow.agent_approved )
    {
      _db.adjust_balance( o.agent, escrow.get_fee() );

      push_virtual_operation( _db, escrow_approved_operation( o.from, o.to, o.agent,
        o.escrow_id, escrow.get_fee() ) );

      _db.modify( escrow, [&]( escrow_object& esc )
      {
        esc.pending_fee.amount = 0;
      });
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
    FC_ASSERT( _db.head_block_time() < e.escrow_expiration, "Disputing the escrow must happen before expiration." );
    FC_ASSERT( e.to_approved && e.agent_approved && "The escrow must be approved by all parties before a dispute can be raised." );
    FC_ASSERT( !e.disputed, "The escrow is already under dispute." );
    FC_ASSERT( e.to == o.to && "dispute", "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", e.to) );
    FC_ASSERT( e.agent == o.agent && "dispute", "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", e.agent) );

    _db.modify( e, [&]( escrow_object& esc )
    {
      esc.disputed = true;
    });
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

    FC_ASSERT( e.get_hive_balance() >= o_hive_amount, "Release amount exceeds escrow balance. Amount: ${a}, Balance: ${b}", ("a", o_hive_amount)("b", e.get_hive_balance()) );
    FC_ASSERT( e.get_hbd_balance() >= o_hbd_amount, "Release amount exceeds escrow balance. Amount: ${a}, Balance: ${b}", ("a", o_hbd_amount)("b", e.get_hbd_balance()) );
    FC_ASSERT( e.to == o.to && "release", "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o", o.to)("e", e.to) );
    FC_ASSERT( e.agent == o.agent && "release", "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o", o.agent)("e", e.agent) );
    FC_ASSERT( o.receiver == e.from || o.receiver == e.to, "Funds must be released to 'from' (${f}) or 'to' (${t})", ("f", e.from)("t", e.to) );
    FC_ASSERT( e.to_approved && e.agent_approved && "Funds cannot be released prior to escrow approval." );

    // If there is a dispute regardless of expiration, the agent can release funds to either party
    if( e.disputed )
    {
      FC_ASSERT( o.who == e.agent, "Only 'agent' (${a}) can release funds in a disputed escrow.", ("a", e.agent) );
    }
    else
    {
      FC_ASSERT( o.who == e.from || o.who == e.to, "Only 'from' (${f}) and 'to' (${t}) can release funds from a non-disputed escrow", ("f", e.from)("t", e.to) );

      if( e.escrow_expiration > _db.head_block_time() )
      {
        // If there is no dispute and escrow has not expired, either party can release funds to the other.
        if( o.who == e.from )
        {
          FC_ASSERT( o.receiver == e.to, "Only 'from' (${f}) can release funds to 'to' (${t}).", ("f", e.from)("t", e.to) );
        }
        else if( o.who == e.to )
        {
          FC_ASSERT( o.receiver == e.from, "Only 'to' (${t}) can release funds to 'from' (${t}).", ("f", e.from)("t", e.to) );
        }
      }
    }
    // If escrow expires and there is no dispute, either party can release funds to either party.

    _db.adjust_balance( o.receiver, o_hive_amount );
    _db.adjust_balance( o.receiver, o_hbd_amount );

    _db.modify( e, [&]( escrow_object& esc )
    {
      esc.hive_balance -= o_hive_amount;
      esc.hbd_balance -= o_hbd_amount;
    } );

    if( e.get_hive_balance().amount == 0 && e.get_hbd_balance().amount == 0 )
    {
      _db.modify( from_account, []( account_object& a )
      {
        a.set_pending_escrow_transfers( a.get_pending_escrow_transfers() - 1 );
      } );
      _db.remove( e );
    }
  }
  FC_CAPTURE_AND_RETHROW( (o) )
}

void transfer_evaluator::do_apply( const transfer_operation& o )
{
  if ( _db.has_hardfork(HIVE_HARDFORK_1_24) && o.amount.symbol == HIVE_SYMBOL && _db.is_treasury( o.to ) ) {
    const auto &fhistory = _db.get_feed_history();

    FC_ASSERT(!fhistory.current_median_history.is_null(), "Cannot send HIVE to ${s} because there is no price feed.", ("s", o.to ));

    auto amount_to_transfer = o.amount * fhistory.current_median_history;

    _db.adjust_balance(o.from, -o.amount);
    _db.adjust_balance(o.to, amount_to_transfer);

    _db.adjust_supply(-o.amount);

    if (amount_to_transfer.amount > 0)
      _db.adjust_supply(amount_to_transfer);

    // o.to will always be the treasury so no need to call _db.get_treasury
    push_virtual_operation( _db, dhf_conversion_operation( o.to, o.amount, amount_to_transfer ) );
    return;
  }
  else if( _db.has_hardfork( HIVE_HARDFORK_0_21__3343 ) )
  {
    FC_ASSERT( o.amount.symbol == HBD_SYMBOL || !_db.is_treasury( o.to ), "Can only transfer HBD or HIVE to ${s}", ("s", o.to ) );
  }

  _db.adjust_balance( o.from, -o.amount );
  _db.adjust_balance( o.to, o.amount );
}

void transfer_to_vesting_evaluator::do_apply( const transfer_to_vesting_operation& o )
{
  const auto& from_account = _db.get_account(o.from);
  const auto& to_account = o.to.size() ? _db.get_account(o.to) : from_account;

  if( _db.has_hardfork( HIVE_HARDFORK_0_21__3343 ) )
  {
    FC_ASSERT( (o.amount.symbol == HBD_SYMBOL || !_db.is_treasury( o.to )),
      "Can only transfer HBD to ${s}", ("s", o.to ) );
  }

  _db.adjust_balance( from_account, -o.amount );

  asset amount_vested;

  /*
    Voting immediately when `transfer_to_vesting` is executed is dangerous,
    because malicious accounts would take over whole HIVE network.
    Therefore an idea is based on voting deferring. Default value is 30 days.
    This range of time is enough long to defeat/block potential malicious intention.
  */
  if( _db.has_hardfork( HIVE_HARDFORK_1_24 ) )
  {
    amount_vested = _db.adjust_account_vesting_balance( to_account, o.amount, false/*to_reward_balance*/, []( asset vests_created ) {} );

    delayed_voting dv( _db );
    dv.add_delayed_value( to_account, _db.head_block_time(), amount_vested.amount.value );
  }
  else
  {
    amount_vested = _db.create_vesting( to_account, o.amount );
  }

  /// Emit this vop unconditionally, since VESTS balance changed immediately, indepdenent to subsequent updates of account voting power done inside `delayed_voting` mechanism.
  push_virtual_operation( _db, transfer_to_vesting_completed_operation(from_account.get_name(), to_account.get_name(), o.amount, amount_vested) );
}

void withdraw_vesting_evaluator::do_apply( const withdraw_vesting_operation& o )
{
  const auto& account = _db.get_account( o.account );
  const auto& account_assets = _db.get< assets_object >( assets_object::id_type( account.get_id().get_value() ) );
  const auto& account_time = _db.get< time_object >( time_object::id_type( account.get_id().get_value() ) );
  const auto& account_mrc = _db.get< manabars_rc_object >( manabars_rc_object::id_type( account.get_id().get_value() ) );
  auto now = _db.head_block_time();

  VEST_asset o_vesting_shares = o.get_vesting_shares();

  if( o_vesting_shares.amount < 0 )
  {
    FC_ASSERT( !_db.has_hardfork( HIVE_HARDFORK_0_20 ), "Cannot withdraw negative VESTS. account: ${account}, vests:${vests}",
      ( "account", o.account )( "vests", o_vesting_shares ) );
    //see https://peakd.com/imfamy/@ironshield/block-23847548-the-block-which-will-live-in-infamy
    return;
  }

  FC_ASSERT( account_assets.get_vesting() >= VEST_asset( 0 ), "Account does not have sufficient Hive Power for withdraw." );
  FC_ASSERT( account_assets.get_vesting() - account_assets.get_delegated_vesting() >= o_vesting_shares, "Account does not have sufficient Hive Power for withdraw." );

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    _db.rc().regenerate_rc_mana( account, account_mrc, account_assets, account_time, now );
  if( o_vesting_shares.amount == 0 )
  {
    if( _db.has_hardfork( HIVE_HARDFORK_1_28_FIX_CANCEL_POWER_DOWN ) )
      FC_ASSERT( account_time.has_active_power_down(), "This operation would not change the vesting withdraw rate." );

    _db.modify( account_assets, [&]( assets_object& a )
    {
      a.set_vesting_withdraw_rate( VEST_asset( 0 ) );
      a.set_to_withdraw( VEST_asset( 0 ) );
      a.set_withdrawn( VEST_asset( 0 ) );
    } );
    _db.modify( account_time, [&]( time_object& t )
    {
      t.set_next_vesting_withdrawal( time_point_sec::maximum() );
    } );
  }
  else
  {
    int vesting_withdraw_intervals = HIVE_VESTING_WITHDRAW_INTERVALS_PRE_HF_16;
    if( _db.has_hardfork( HIVE_HARDFORK_0_16__551 ) )
      vesting_withdraw_intervals = HIVE_VESTING_WITHDRAW_INTERVALS; /// 13 weeks = 1 quarter of a year

    VEST_asset new_vesting_withdraw_rate = o_vesting_shares / vesting_withdraw_intervals;

    if( new_vesting_withdraw_rate.amount == 0 )
      new_vesting_withdraw_rate.amount = 1;

    if( _db.has_hardfork( HIVE_HARDFORK_0_21 ) && new_vesting_withdraw_rate.amount * vesting_withdraw_intervals < o_vesting_shares.amount )
    {
      new_vesting_withdraw_rate.amount += 1;
    }

    if( _db.has_hardfork( HIVE_HARDFORK_1_28_FIX_CANCEL_POWER_DOWN ) )
      FC_ASSERT( account_assets.get_vesting_withdraw_rate() != new_vesting_withdraw_rate, "This operation would not change the vesting withdraw rate." );

    _db.modify( account_assets, [&]( assets_object& a )
    {
      a.set_vesting_withdraw_rate( new_vesting_withdraw_rate );
      a.set_to_withdraw( o_vesting_shares );
      a.set_withdrawn( VEST_asset( 0 ) );
    } );
    _db.modify( account_time, [&]( time_object& t )
    {
      t.set_next_vesting_withdrawal( now + fc::seconds( HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS ) );
    } );
  }
  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    _db.rc().update_account_after_vest_change( account, account_mrc, account_assets, account_time, now, false, true );
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
    FC_ASSERT( !_db.is_treasury( o.to_account ), "Cannot withdraw vesting to ${s}", ("s", o.to_account ) );
  }

  if( itr == wd_idx.end() )
  {
    FC_ASSERT( o.percent != 0, "Cannot create a 0% destination." );
    FC_ASSERT( from_account.get_withdraw_routes() < HIVE_MAX_WITHDRAW_ROUTES, "Account already has the maximum number of routes." );

    _db.create< withdraw_vesting_route_object >( [&]( withdraw_vesting_route_object& wvdo )
    {
      wvdo.from_account = from_account.get_name();
      wvdo.to_account = to_account.get_name();
      wvdo.percent = o.percent;
      wvdo.auto_vest = o.auto_vest;
    });

    _db.modify( from_account, [&]( account_object& a )
    {
      a.set_withdraw_routes( a.get_withdraw_routes() + 1 );
    });
  }
  else if( o.percent == 0 )
  {
    _db.remove( *itr );

    _db.modify( from_account, [&]( account_object& a )
    {
      a.set_withdraw_routes( a.get_withdraw_routes() - 1 );
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

  FC_ASSERT( total_percent <= HIVE_100_PERCENT, "More than 100% of vesting withdrawals allocated to destinations." );
  }
  FC_CAPTURE_AND_RETHROW()
}

void convert_evaluator::do_apply( const convert_operation& o )
{
  const auto& owner = _db.get_account( o.owner );
  HBD_asset o_amount = o.get_amount();
  _db.adjust_balance( owner, -o_amount );

  const auto& fhistory = _db.get_feed_history();
  FC_ASSERT( !fhistory.current_median_history.is_null() && "Cannot convert HBD because there is no price feed." );

  auto hive_conversion_delay = HIVE_CONVERSION_DELAY_PRE_HF_16;
  if( _db.has_hardfork( HIVE_HARDFORK_0_16__551) )
    hive_conversion_delay = HIVE_CONVERSION_DELAY;

  _db.create<convert_request_object>( owner, o_amount, _db.head_block_time() + hive_conversion_delay, o.requestid );
}

void collateralized_convert_evaluator::do_apply( const collateralized_convert_operation& o )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_25 ) && "Operation not available until HF25" );

  const auto& owner = _db.get_account( o.owner );
  HIVE_asset o_amount = o.get_amount();
  _db.adjust_balance( owner, -o_amount );

  const auto& fhistory = _db.get_feed_history();
  FC_ASSERT( !fhistory.current_median_history.is_null() && "Cannot convert HIVE because there is no price feed." );
  const auto& dgpo = _db.get_dynamic_global_properties();
  FC_ASSERT( dgpo.hbd_print_rate > 0, "Creation of new HBD blocked at this time due to global limit." );
    //note that feed and therefore print rate is updated once per hour, so without above check there could be
    //enough room for new HBD, but there is a chance the official price would still be artificial (artificial
    //price is not used in this conversion, but users might think it is - better to stop them from making mistake)

  //if you change something here take a look at wallet_api::estimate_hive_collateral as well

  //cut amount by collateral ratio
  uint128_t _amount = ( uint128_t( o_amount.amount.value ) * HIVE_100_PERCENT ) / HIVE_CONVERSION_COLLATERAL_RATIO;
  HIVE_asset for_immediate_conversion = HIVE_asset( fc::uint128_to_uint64( _amount ) );

  //immediately create HBD - apply fee to current rolling minimum price
  HBD_asset converted_amount = multiply_with_fee( for_immediate_conversion, fhistory.current_min_history,
    HIVE_COLLATERALIZED_CONVERSION_FEE, HIVE_SYMBOL );
  FC_ASSERT( converted_amount.amount > 0, "Amount of collateral too low - conversion gives no HBD" );
  _db.adjust_balance( owner, converted_amount );

  _db.modify( dgpo, [&]( dynamic_global_property_object& p )
  {
    //HIVE supply (and virtual supply in part related to HIVE) will be corrected after actual conversion
    p.current_hbd_supply += converted_amount;
    p.virtual_supply += converted_amount * fhistory.current_median_history;
  } );
  //note that we created new HBD out of thin air and we will burn the related HIVE when actual conversion takes
  //place; there is alternative approach - we could burn all collateral now and print back excess when we are
  //to return it to the user; the difference slightly changes calculations below, that is, currently we might
  //allow slightly more HBD to be printed

  uint16_t percent_hbd = _db.calculate_HBD_percent();
  FC_ASSERT( percent_hbd <= dgpo.hbd_stop_percent, "Creation of new ${hbd} violates global limit.", ( "hbd", converted_amount ) );

  _db.create<collateralized_convert_request_object>( owner, o_amount, converted_amount,
    _db.head_block_time() + HIVE_COLLATERALIZED_CONVERSION_DELAY, o.requestid );

  push_virtual_operation( _db, collateralized_convert_immediate_conversion_operation( o.owner, o.requestid, converted_amount ) );
}

void limit_order_create_evaluator::do_apply( const limit_order_create_operation& o )
{
  FC_ASSERT( o.expiration > _db.head_block_time(), "Limit order has to expire after head block time." );

  time_point_sec expiration = o.expiration;
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1449 ) ) // 307842c657d54f576615cd312a425c517ad68db4 example tx with extra long expiration
  {
    FC_ASSERT( o.expiration <= _db.head_block_time() + HIVE_MAX_LIMIT_ORDER_EXPIRATION, "Limit Order Expiration must not be more than 28 days in the future" );
  }
#ifndef HIVE_CONVERTER_BUILD // limit_order_object expiration time is explicitly set during the conversion time before HF20, due to the altered block id
  else
  {
    uint32_t rand_offset = _db.head_block_id()._hash[ 4 ] % 86400;
    expiration = std::min( o.expiration, fc::time_point_sec( HIVE_HARDFORK_0_20_TIME + HIVE_MAX_LIMIT_ORDER_EXPIRATION + rand_offset ) );
  }
#endif

  _db.adjust_balance( o.owner, -o.amount_to_sell );
  const auto& order = _db.create<limit_order_object>( o.owner, o.amount_to_sell, o.get_price(), _db.head_block_time(), expiration, o.orderid );

  bool filled = _db.apply_order( order );

  FC_ASSERT( not o.fill_or_kill || filled, "Cancelling order because it was not filled." );
}

void limit_order_create2_evaluator::do_apply( const limit_order_create2_operation& o )
{
  FC_ASSERT( o.expiration > _db.head_block_time() && "Limit order has to expire after head block time." );

  time_point_sec expiration = o.expiration;
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__1449 ) ) // 8bce7ca4b2bea9af848db927342a88f82eea0684 example tx with extra long expiration
  {
    FC_ASSERT( o.expiration <= _db.head_block_time() + HIVE_MAX_LIMIT_ORDER_EXPIRATION && "Limit Order Expiration must not be more than 28 days in the future" );
  }
  else
  {
    expiration = std::min( o.expiration, fc::time_point_sec( HIVE_HARDFORK_0_20_TIME + HIVE_MAX_LIMIT_ORDER_EXPIRATION ) );
  }

  _db.adjust_balance( o.owner, -o.amount_to_sell );
  const auto& order = _db.create<limit_order_object>( o.owner, o.amount_to_sell, o.exchange_rate, _db.head_block_time(), expiration, o.orderid );

  bool filled = _db.apply_order( order );

  if( o.fill_or_kill ) FC_ASSERT( filled && "Cancelling order because it was not filled." );
}

void limit_order_cancel_evaluator::do_apply( const limit_order_cancel_operation& o )
{
  _db.cancel_order( _db.get_limit_order( o.owner, o.orderid ) );
}

void transfer_to_savings_evaluator::do_apply( const transfer_to_savings_operation& op )
{
  const auto& from = _db.get_account( op.from );
  const auto& to   = _db.get_account(op.to);

  FC_ASSERT( !_db.is_treasury( op.to ), "Cannot transfer to savings of treasury ${s}", ("s", op.to ) );

  _db.adjust_balance( from, -op.amount );
  _db.adjust_savings_balance( to, op.amount );
}

void transfer_from_savings_evaluator::do_apply( const transfer_from_savings_operation& op )
{
  const auto& from = _db.get_account( op.from );
  _db.get_account(op.to); // Verify to account exists

  FC_ASSERT( from.get_savings_withdraw_requests() < HIVE_SAVINGS_WITHDRAW_REQUEST_LIMIT, "Account has reached limit for pending withdraw requests." );

  FC_ASSERT( op.amount.symbol == HBD_SYMBOL || !_db.is_treasury( op.to ), "Can only transfer HBD to ${s}", ("s", op.to ) );

  FC_ASSERT( _db.get_savings_balance( from, op.amount.symbol ) >= op.amount );
  _db.adjust_savings_balance( from, -op.amount );
  _db.create<savings_withdraw_object>( op.from, op.to, op.amount, op.memo, _db.head_block_time() + HIVE_SAVINGS_WITHDRAW_TIME, op.request_id );

  _db.modify( from, [&]( account_object& a )
  {
    a.set_savings_withdraw_requests( a.get_savings_withdraw_requests() + 1 );
  } );
}

void cancel_transfer_from_savings_evaluator::do_apply( const cancel_transfer_from_savings_operation& op )
{
  const auto& swo = _db.get_savings_withdraw( op.from, op.request_id );
  _db.adjust_savings_balance( _db.get_account( swo.from ), swo.amount );
  _db.remove( swo );

  const auto& from = _db.get_account( op.from );
  _db.modify( from, [&]( account_object& a )
  {
    a.set_savings_withdraw_requests( a.get_savings_withdraw_requests() - 1 );
  } );
}

void claim_reward_balance_evaluator::do_apply( const claim_reward_balance_operation& op )
{
  const auto& acnt = _db.get_account( op.account );
  const auto& acnt_assets = _db.get< assets_object >( assets_object::id_type( acnt.get_id().get_value() ) );
  const auto& acnt_time = _db.get< time_object >( time_object::id_type( acnt.get_id().get_value() ) );
  const auto& acnt_mrc = _db.get< manabars_rc_object >( manabars_rc_object::id_type( acnt.get_id().get_value() ) );
  const auto& dgpo = _db.get_dynamic_global_properties();

  // Verify 1:1 ID correspondence between account and split objects
  FC_ASSERT( acnt_assets.get_account_id() == acnt.get_id(),
    "ID mismatch: assets_object account_id ${assets_acc_id} != account.get_id() ${acc_id} for account ${name}",
    ( "assets_acc_id", acnt_assets.get_account_id() )( "acc_id", acnt.get_id() )( "name", op.account ) );
  auto now = dgpo.time;

  HIVE_asset op_reward_hive = op.get_reward_hive();
  HBD_asset op_reward_hbd = op.get_reward_hbd();
  VEST_asset op_reward_vests = op.get_reward_vests();

  FC_ASSERT( op_reward_hive <= acnt_assets.get_rewards(), "Cannot claim that much HIVE. Claim: ${c} Actual: ${a}",
    ( "c", op_reward_hive )( "a", acnt_assets.get_rewards() ) );
  FC_ASSERT( op_reward_hbd <= acnt_assets.get_hbd_rewards(), "Cannot claim that much HBD. Claim: ${c} Actual: ${a}",
    ( "c", op_reward_hbd )( "a", acnt_assets.get_hbd_rewards() ) );
  FC_ASSERT( op_reward_vests <= acnt_assets.get_vest_rewards(), "Cannot claim that much VESTS. Claim: ${c} Actual: ${a}",
    ( "c", op_reward_vests )( "a", acnt_assets.get_vest_rewards() ) );

  HIVE_asset reward_vesting_hive_to_move = HIVE_asset( 0 );
  if( op_reward_vests == acnt_assets.get_vest_rewards() )
    reward_vesting_hive_to_move = acnt_assets.get_vest_rewards_as_hive();
  else
    reward_vesting_hive_to_move = HIVE_asset( fc::uint128_to_uint64( ( uint128_t( op_reward_vests.amount.value ) * uint128_t( acnt_assets.get_vest_rewards_as_hive().amount.value ) )
      / uint128_t( acnt_assets.get_vest_rewards().amount.value ) ) );

  _db.adjust_reward_balance( acnt, -op_reward_hive );
  _db.adjust_reward_balance( acnt, -op_reward_hbd );
  _db.adjust_balance( acnt, op_reward_hive );
  _db.adjust_balance( acnt, op_reward_hbd );

  _db.modify( acnt_assets, [&]( assets_object& a )
  {
    if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    {
      _db.modify( acnt_mrc, [&]( manabars_rc_object& mrc )
      {
        _db.modify( acnt_time, [&]( time_object& t )
        {
          util::update_manabar( dgpo, acnt, a, t, mrc, op_reward_vests.amount.value );
        } );
        a.set_vesting( a.get_vesting() + op_reward_vests );
        a.set_vest_rewards( a.get_vest_rewards() - op_reward_vests );
        a.set_vest_rewards_as_hive( a.get_vest_rewards_as_hive() - reward_vesting_hive_to_move );
      } );
      _db.rc().regenerate_rc_mana( acnt, acnt_mrc, acnt_assets, acnt_time, now );
    }

    a.set_vesting( a.get_vesting() + op_reward_vests );
    a.set_vest_rewards( a.get_vest_rewards() - op_reward_vests );
    a.set_vest_rewards_as_hive( a.get_vest_rewards_as_hive() - reward_vesting_hive_to_move );
  } );
  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    _db.rc().update_account_after_vest_change( acnt, now );

  _db.modify( dgpo, [&]( dynamic_global_property_object& gpo )
  {
    gpo.total_vesting_shares += op_reward_vests;
    gpo.total_vesting_fund_hive += reward_vesting_hive_to_move;

    gpo.pending_rewarded_vesting_shares -= op_reward_vests;
    gpo.pending_rewarded_vesting_hive -= reward_vesting_hive_to_move;
  } );

  _db.adjust_proxied_witness_votes( acnt, op_reward_vests.amount );
}

#ifdef HIVE_ENABLE_SMT
void claim_reward_balance2_evaluator::do_apply( const claim_reward_balance2_operation& op )
{
  //TODO: can be removed once SMT hardfork activates
  FC_ASSERT( _db.has_hardfork( HIVE_SMT_HARDFORK ) && "Premature", "claim_reward_balance2_operation requires hardfork ${x}",
    ( "x", HIVE_SMT_HARDFORK ) );
  const account_object* a = nullptr; // Lazily initialized below because it may turn out unnecessary.
  const assets_object* a_assets = nullptr;

  for( const asset& token : op.reward_tokens )
  {
    if( token.amount == 0 )
      continue;

    if( token.symbol.space() == asset_symbol_type::smt_nai_space )
    {
      _db.adjust_reward_balance( op.account, -token );
      _db.adjust_balance( op.account, token );
    }
    else
    {
      // Lazy init here.
      if( a == nullptr )
      {
        a = _db.find_account( op.account );
        FC_ASSERT( a != nullptr && "Not found", "Could NOT find account ${a}", ("a", op.account) );
        a_assets = &_db.get< assets_object >( assets_object::id_type( a->get_id().get_value() ) );
      }

      if( token.symbol == VESTS_SYMBOL )
      {
        FC_ASSERT( token <= a_assets->get_vest_rewards(), "Cannot claim that much VESTS. Claim: ${c} Actual: ${a}",
          ( "c", token )( "a", a_assets->get_vest_rewards() ) );

        const auto& dgpo = _db.get_dynamic_global_properties();
        auto now = dgpo.time;

        asset reward_vesting_hive_to_move = asset( 0, HIVE_SYMBOL );
        if( token == a_assets->get_vest_rewards() )
          reward_vesting_hive_to_move = a_assets->get_vest_rewards_as_hive();
        else
          reward_vesting_hive_to_move = asset( fc::uint128_to_uint64( ( uint128_t( token.amount.value ) * uint128_t( a_assets->get_vest_rewards_as_hive().amount.value ) )
            / uint128_t( a_assets->get_vest_rewards().amount.value ) ), HIVE_SYMBOL );

        const auto& a_mrc = _db.get< manabars_rc_object >( manabars_rc_object::id_type( a->get_id().get_value() ) );
        const auto& a_time = _db.get< time_object >( time_object::id_type( a->get_id().get_value() ) );
        _db.rc().regenerate_rc_mana( *a, a_mrc, *a_assets, a_time, now );
        _db.modify( *a_assets, [&]( assets_object& assets )
        {
          assets.set_vesting( assets.get_vesting() + token );
          assets.set_vest_rewards( assets.get_vest_rewards() - token );
          assets.set_vest_rewards_as_hive( assets.get_vest_rewards_as_hive() - reward_vesting_hive_to_move );
        } );
        _db.rc().update_account_after_vest_change( *a, a_mrc, *a_assets, a_time, now );

        _db.modify( dgpo, [&]( dynamic_global_property_object& gpo )
        {
          gpo.total_vesting_shares += token;
          gpo.total_vesting_fund_hive += reward_vesting_hive_to_move;

          gpo.pending_rewarded_vesting_shares -= token;
          gpo.pending_rewarded_vesting_hive -= reward_vesting_hive_to_move;
        } );

        _db.adjust_proxied_witness_votes( *a, token.amount );
      }
      else if( token.symbol == HIVE_SYMBOL || token.symbol == HBD_SYMBOL )
      {
        FC_ASSERT( is_asset_type( token, HIVE_SYMBOL ) == false || token <= a_assets->get_rewards(),
          "Cannot claim that much HIVE. Claim: ${c} Actual: ${a}", ( "c", token )( "a", a_assets->get_rewards() ) );
        FC_ASSERT( is_asset_type( token, HBD_SYMBOL ) == false || token <= a_assets->get_hbd_rewards(),
          "Cannot claim that much HBD. Claim: ${c} Actual: ${a}", ( "c", token )( "a", a_assets->get_hbd_rewards() ) );
        _db.adjust_reward_balance( *a, -token );
        _db.adjust_balance( *a, token );
      }
      else
        FC_ASSERT( false && "Unknown asset symbol" );
    } // non-SMT token
  } // for( const auto& token : op.reward_tokens )
}
#endif

void delegate_vesting_shares_evaluator::do_apply( const delegate_vesting_shares_operation& op )
{
  FC_TODO("Update get_effective_vesting_shares when modifying this operation to support SMTs." )

  const auto& delegator = _db.get_account( op.delegator );
  const auto& delegatee = _db.get_account( op.delegatee );
  const auto& delegator_assets = _db.get< assets_object >( assets_object::id_type( delegator.get_id().get_value() ) );
  const auto& delegatee_assets = _db.get< assets_object >( assets_object::id_type( delegatee.get_id().get_value() ) );
  const auto& delegator_time = _db.get< time_object >( time_object::id_type( delegator.get_id().get_value() ) );
  const auto& delegatee_time = _db.get< time_object >( time_object::id_type( delegatee.get_id().get_value() ) );
  const auto& delegator_mrc = _db.get< manabars_rc_object >( manabars_rc_object::id_type( delegator.get_id().get_value() ) );
  const auto& delegatee_mrc = _db.get< manabars_rc_object >( manabars_rc_object::id_type( delegatee.get_id().get_value() ) );
  auto* delegation = _db.find< vesting_delegation_object, by_delegation >( boost::make_tuple( delegator.get_id(), delegatee.get_id() ) );

  const auto& gpo = _db.get_dynamic_global_properties();
  auto now = gpo.time;

  VEST_asset op_vesting_shares = op.get_vesting_shares();
  VEST_asset available_shares;
  VEST_asset available_downvote_shares;

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
  {
    _db.rc().regenerate_rc_mana( delegator, delegator_mrc, delegator_assets, delegator_time, now );
    _db.rc().regenerate_rc_mana( delegatee, delegatee_mrc, delegatee_assets, delegatee_time, now );
  }

  if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
  {
    auto max_mana = delegator.get_effective_vesting_shares( delegator_assets, delegator_time );

    _db.modify( delegator_mrc, [&]( manabars_rc_object& mrc )
    {
      util::update_manabar( gpo, delegator, delegator_assets, delegator_time, mrc );
    } );

    available_shares = VEST_asset( delegator_mrc.get_voting_manabar().current_mana );
    if( gpo.downvote_pool_percent )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
      {
        available_downvote_shares = VEST_asset(
          fc::uint128_to_int64( ( uint128_t( delegator_mrc.get_downvote_manabar().current_mana ) * HIVE_100_PERCENT ) / gpo.downvote_pool_percent
          + ( HIVE_100_PERCENT / gpo.downvote_pool_percent ) - 1 ) );
      }
      else
      {
        available_downvote_shares = VEST_asset(
          ( delegator_mrc.get_downvote_manabar().current_mana * HIVE_100_PERCENT ) / gpo.downvote_pool_percent
          + ( HIVE_100_PERCENT / gpo.downvote_pool_percent ) - 1 );
      }
    }
    else
    {
      available_downvote_shares = available_shares;
    }

    // Assume delegated VESTS are used first when consuming mana. You cannot delegate received vesting shares
    available_shares.amount = std::min( available_shares.amount, max_mana - delegator_assets.get_received_vesting().amount );
    available_downvote_shares.amount = std::min( available_downvote_shares.amount, max_mana - delegator_assets.get_received_vesting().amount );

    if( delegator_time.get_next_vesting_withdrawal() < fc::time_point_sec::maximum()
      && delegator_assets.get_total_vesting_withdrawal() > delegator_assets.get_vesting_withdraw_rate().amount )
    {
      /*
      Account cannot delegate **any** VESTS that they are powering down. Therefore we have to reduce
      available shares by whole remaining power down. However current voting mana does not include current
      week's power down, so we have to add it, otherwise it would be effectively subtracted twice. We can
      skip that step when power down is in last week, because then whole power down (subtracting) equals
      current week's power down (adding).
      */
      auto weekly_withdraw = delegator_assets.get_vesting_withdraw_rate().amount;
      auto remaining_withdraw = delegator_assets.get_total_vesting_withdrawal();
      available_shares += VEST_asset( weekly_withdraw - remaining_withdraw );
      available_downvote_shares += VEST_asset( weekly_withdraw - remaining_withdraw );
    }
  }
  else
  {
    available_shares = delegator_assets.get_vesting() - delegator_assets.get_delegated_vesting() - VEST_asset( delegator_assets.get_total_vesting_withdrawal() );
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
    FC_ASSERT( available_shares >= op_vesting_shares, "Account ${acc} does not have enough mana to delegate. required: ${r} available: ${a}",
      ( "acc", op.delegator )( "r", op_vesting_shares )( "a", available_shares ) );
    FC_ASSERT( available_downvote_shares >= op_vesting_shares, "Account ${acc} does not have enough downvote mana to delegate. required: ${r} available: ${a}",
      ( "acc", op.delegator )( "r", op_vesting_shares )( "a", available_downvote_shares ) );
    FC_ASSERT( op_vesting_shares >= min_delegation && "Minimum not reached", "Account must delegate a minimum of ${v}", ( "v", min_delegation ) );

    _db.create< vesting_delegation_object >( delegator, delegatee, op_vesting_shares, now );

    _db.modify( delegator_assets, [&]( assets_object& a )
    {
      a.set_delegated_vesting( a.get_delegated_vesting() + op_vesting_shares );
    } );

    if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
    {
      _db.modify( delegator_mrc, [&]( manabars_rc_object& mrc )
      {
        mrc.get_voting_manabar().use_mana( op_vesting_shares.amount.value );

        if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
        {
          mrc.get_downvote_manabar().use_mana( fc::uint128_to_int64( ( uint128_t( op_vesting_shares.amount.value ) * gpo.downvote_pool_percent ) / HIVE_100_PERCENT ) );
        }
        else if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) )
        {
          mrc.get_downvote_manabar().use_mana( op_vesting_shares.amount.value );
        }
      } );
    }

    _db.modify( delegatee_mrc, [&]( manabars_rc_object& mrc )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        util::update_manabar( gpo, delegatee, delegatee_assets, delegatee_time, mrc, op_vesting_shares.amount.value );
      }

      _db.modify( delegatee_assets, [&]( assets_object& a )
      {
        a.set_received_vesting( a.get_received_vesting() + op_vesting_shares );
      } );
    } );
  }
  else if( op_vesting_shares >= delegation->get_vesting() ) // delegation is increasing
  {
    auto delta = op_vesting_shares - delegation->get_vesting();

    FC_ASSERT( delta >= min_update && "Wrong increase", "Hive Power increase is not enough of a difference. min_update: ${min}", ("min", min_update) );
    FC_ASSERT( available_shares >= delta, "Account ${acc} does not have enough mana to delegate. required: ${r} available: ${a}",
      ( "acc", op.delegator )( "r", delta )( "a", available_shares ) );
    FC_ASSERT( available_downvote_shares >= delta, "Account ${acc} does not have enough downvote mana to delegate. required: ${r} available: ${a}",
      ( "acc", op.delegator )( "r", delta )( "a", available_downvote_shares ) );

    _db.modify( delegator_assets, [&]( assets_object& a )
    {
      a.set_delegated_vesting( a.get_delegated_vesting() + delta );
    } );

    if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
    {
      _db.modify( delegator_mrc, [&]( manabars_rc_object& mrc )
      {
        mrc.get_voting_manabar().use_mana( delta.amount.value );

        if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
        {
          mrc.get_downvote_manabar().use_mana( fc::uint128_to_int64( ( uint128_t( delta.amount.value ) * gpo.downvote_pool_percent ) / HIVE_100_PERCENT ) );
        }
        else if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) )
        {
          mrc.get_downvote_manabar().use_mana( delta.amount.value );
        }
      } );
    }

    _db.modify( delegatee_mrc, [&]( manabars_rc_object& mrc )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        util::update_manabar( gpo, delegatee, delegatee_assets, delegatee_time, mrc, delta.amount.value );
      }
      _db.modify( delegatee_assets, [&]( assets_object& a )
      {
        a.set_received_vesting( a.get_received_vesting() + delta );
      } );
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
      FC_ASSERT( delta >= min_update && "Wrong decrease", "Hive Power decrease is not enough of a difference. min_update: ${min}", ( "min", min_update ) );
      FC_ASSERT( op_vesting_shares >= min_delegation, "Delegation must be removed or leave minimum delegation amount of ${v}", ( "v", min_delegation ) );
    }
    else
    {
      FC_ASSERT( delegation->get_vesting().amount > 0, "Delegation would set vesting_shares to zero, but it is already zero" );
    }

    _db.create< vesting_delegation_expiration_object >( delegator, delta,
      std::max( now + gpo.delegation_return_period, delegation->get_min_delegation_time() ) );

    _db.modify( delegatee_mrc, [&]( manabars_rc_object& mrc )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
      {
        util::update_manabar( gpo, delegatee, delegatee_assets, delegatee_time, mrc );
      }

     _db.modify( delegatee_assets, [&]( assets_object& a )
      {
        a.set_received_vesting( a.get_received_vesting() - delta );
      } );

      if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        mrc.get_voting_manabar().use_mana( delta.amount.value );

        if( _db.has_hardfork( HIVE_HARDFORK_0_21__3336 ) )
        {
          if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
          {
           mrc.get_downvote_manabar().use_mana( fc::uint128_to_int64( ( uint128_t( delta.amount.value ) * gpo.downvote_pool_percent ) / HIVE_100_PERCENT ) );
          }
          else
          {
            mrc.get_downvote_manabar().use_mana( op_vesting_shares.amount.value );
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
    _db.rc().update_account_after_vest_change( delegator, delegator_mrc, delegator_assets, delegator_time, now, true, true );
    _db.rc().update_account_after_vest_change( delegatee, delegatee_mrc, delegatee_assets, delegatee_time, now, true, true );
  }
}

void recurrent_transfer_evaluator::do_apply( const recurrent_transfer_operation& op )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_25 ), "Recurrent transfers are not enabled until hardfork ${hf}", ("hf", HIVE_HARDFORK_1_25) );

  const auto& from_account = _db.get_account( op.from );
  const auto& to_account = _db.get_account( op.to );

  asset available = _db.get_balance( from_account, op.amount.symbol );
  FC_ASSERT( available >= op.amount, "Account does not have enough tokens for the first transfer, has ${has} needs ${needs}", ("has",  available)("needs", op.amount) );

  uint8_t rtp_id = op.get_pair_id();

  const auto& rt_idx = _db.get_index< recurrent_transfer_index, by_from_to_id >();
  auto itr = rt_idx.find( boost::make_tuple( from_account.get_id(), to_account.get_id(), rtp_id ) );

  if( itr == rt_idx.end() )
  {
    FC_ASSERT( from_account.get_open_recurrent_transfers() < HIVE_MAX_OPEN_RECURRENT_TRANSFERS, "Account can't have more than ${rt} recurrent transfers", ( "rt", HIVE_MAX_OPEN_RECURRENT_TRANSFERS ) );
    // If the recurrent transfer is not found and the amount is 0 it means the user wants to delete a transfer that doesn't exist
    FC_ASSERT( op.amount.amount != 0, "Cannot create a recurrent transfer with 0 amount" );
    const auto& recurrent_transfer = _db.create< recurrent_transfer_object >( _db.head_block_time(), from_account, to_account, op.amount, op.memo, op.recurrence, op.executions, rtp_id );
    FC_ASSERT( recurrent_transfer.get_final_trigger_date() <= _db.head_block_time() + fc::days( HIVE_MAX_RECURRENT_TRANSFER_END_DATE ),
      "Cannot set a transfer that would last for longer than ${days} days", ( "days", HIVE_MAX_RECURRENT_TRANSFER_END_DATE ) );

    _db.modify(from_account, [](account_object& a )
    {
      a.set_open_recurrent_transfers( a.get_open_recurrent_transfers() + 1 );
    } );
  }
  else if( op.amount.amount == 0 )
  {
    _db.remove( *itr );
    _db.modify( from_account, [&](account_object& a )
    {
      FC_ASSERT( a.get_open_recurrent_transfers() > 0 && "No (more) open recurrent transfers" );
      a.set_open_recurrent_transfers( a.get_open_recurrent_transfers() - 1 );
    } );
  }
  else
  {
    const auto& recurrent_transfer = *itr;
    _db.modify( recurrent_transfer, [&]( recurrent_transfer_object& rt )
    {
      rt.amount = op.amount;
      from_string( rt.memo, op.memo );
      rt.set_recurrence_trigger_date( _db.head_block_time(), op.recurrence );
      rt.remaining_executions = op.executions;
    } );
    FC_ASSERT( recurrent_transfer.get_final_trigger_date() <= _db.head_block_time() + fc::days( HIVE_MAX_RECURRENT_TRANSFER_END_DATE ) && "too long",
      "Cannot set a transfer that would last for longer than ${days} days", ( "days", HIVE_MAX_RECURRENT_TRANSFER_END_DATE ) );
  }
}

} } // hive::chain

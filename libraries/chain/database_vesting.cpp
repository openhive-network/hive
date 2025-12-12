#include <hive/chain/hardfork_property_object_multiindex.hpp>
#include <hive/chain/detail/state/liquidity_reward_balance_object_multiindex.hpp>
#include <hive/chain/detail/state/withdraw_vesting_route_object_multiindex.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/global_property_object_multiindex.hpp>

#include <hive/chain/database_virtual_operations.hpp>
#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>
#include <hive/chain/util/delayed_voting.hpp>
#include <hive/chain/rc/rc_utility.hpp>

#include <hive/protocol/config.hpp>

namespace hive { namespace chain {

using hive::protocol::liquidity_reward_operation;
using hive::protocol::fill_vesting_withdraw_operation;

void initialize_core_indexes_06( database& db )
{
  HIVE_ADD_CORE_INDEX(db, liquidity_reward_balance_index);
  HIVE_ADD_CORE_INDEX(db, hardfork_property_index);
  HIVE_ADD_CORE_INDEX(db, withdraw_vesting_route_index);
}

void database::pay_liquidity_reward()
{
#ifdef IS_TEST_NET
  if( !liquidity_rewards_enabled )
    return;
#endif

  if( (head_block_num() % HIVE_LIQUIDITY_REWARD_BLOCKS) == 0 )
  {
    auto reward = get_liquidity_reward();

    if( reward.amount == 0 )
      return;

    const auto& ridx = get_index< liquidity_reward_balance_index >().indices().get< by_volume_weight >();
    auto itr = ridx.begin();
    if( itr != ridx.end() && itr->volume_weight() > 0 )
    {
      adjust_supply( reward, true );
      adjust_balance( get(itr->owner), reward );
      modify( *itr, [&]( liquidity_reward_balance_object& obj )
      {
        obj.hive_volume = 0;
        obj.hbd_volume  = 0;
        obj.last_update = head_block_time();
        obj.weight      = 0;
      } );

      push_virtual_operation( *this, liquidity_reward_operation( get(itr->owner).get_name(), reward ) );
    }
  }
}

void database::adjust_liquidity_reward( const account_object& owner, const asset& volume, bool is_hbd )
{
  const auto& ridx = get_index< liquidity_reward_balance_index >().indices().get< by_owner >();
  auto itr = ridx.find( owner.get_id() );
  if( itr != ridx.end() )
  {
    modify<liquidity_reward_balance_object>( *itr, [&]( liquidity_reward_balance_object& r )
    {
      if( head_block_time() - r.last_update >= HIVE_LIQUIDITY_TIMEOUT_SEC )
      {
        r.hbd_volume = 0;
        r.hive_volume = 0;
        r.weight = 0;
      }

      if( is_hbd )
        r.hbd_volume += volume.amount.value;
      else
        r.hive_volume += volume.amount.value;

      r.update_weight( has_hardfork( HIVE_HARDFORK_0_10__141 ) );
      r.last_update = head_block_time();
    } );
  }
  else
  {
    create<liquidity_reward_balance_object>( [&](liquidity_reward_balance_object& r )
    {
      r.owner = owner.get_id();
      if( is_hbd )
        r.hbd_volume = volume.amount.value;
      else
        r.hive_volume = volume.amount.value;

      r.update_weight( has_hardfork( HIVE_HARDFORK_0_9__141 ) );
      r.last_update = head_block_time();
    } );
  }
}

void database::retally_liquidity_weight() {
  const auto& ridx = get_index< liquidity_reward_balance_index >().indices().get< by_owner >();
  for( const auto& i : ridx ) {
    modify( i, []( liquidity_reward_balance_object& o ){
      o.update_weight(true/*HAS HARDFORK10 if this method is called*/);
    });
  }
}

void database::process_vesting_withdrawals()
{
  const auto& widx = get_index< account_index, by_next_vesting_withdrawal >();
  const auto& didx = get_index< withdraw_vesting_route_index, by_withdraw_route >();
  auto current = widx.begin();

  const auto& cprops = get_dynamic_global_properties();
  auto now = cprops.time;

  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( current != widx.end() && current->next_vesting_withdrawal <= now )
  {
    const auto& from_account = *current; ++current; ++count;

    share_type to_withdraw = from_account.get_next_vesting_withdrawal();
    if( !has_hardfork( HIVE_HARDFORK_1_28_FIX_POWER_DOWN ) && to_withdraw < from_account.vesting_withdraw_rate.amount )
      to_withdraw = from_account.to_withdraw.amount % from_account.vesting_withdraw_rate.amount;
    // see history of first (and so far the only) power down of 'gil' account: https://hiveblocks.com/@gil
    // the situation was caused by HF1, where vesting_withdraw_rate changed from 9615 before split to 9615.384615
    // (instead of correct 9615.000000); that is the source of nonequivalence between taking all the rest of power down
    // (0.769270 VESTS) and modulo of "all % weekly rate" (0.000040 VESTS);
    // it is possible that other accounts were also affected in similar way, 'gil' was just the first where the difference
    // occurred
    bool invalid_power_down = to_withdraw > from_account.get_vesting().amount;
    if( invalid_power_down )
    {
      elog( "NOTIFYALERT! somehow account was scheduled to power down more than it has on balance (${s} vs ${h})",
        ( "s", to_withdraw )( "h", from_account.get_vesting().amount ) );
#ifdef USE_ALTERNATE_CHAIN_ID
      FC_ASSERT( not invalid_power_down, "Somehow account was scheduled to power down more than it has on balance (${s} vs ${h})",
        ( "s", to_withdraw )( "h", from_account.get_vesting().amount ) );
#endif
      to_withdraw = from_account.get_vesting().amount;
    }

    optional< delayed_voting > dv;
    delayed_voting::opt_votes_update_data_items _votes_update_data_items;

    if( has_hardfork( HIVE_HARDFORK_1_24 ) )
    {
      dv = delayed_voting( *this );
      _votes_update_data_items = delayed_voting::votes_update_data_items();
    }

    share_type vests_deposited = 0;

    auto process_routing = [ &, this ]( bool auto_vest_mode )
    {
      for( auto itr = didx.upper_bound( boost::make_tuple( from_account.get_name(), account_name_type() ) );
        itr != didx.end() && itr->from_account == from_account.get_name();
        ++itr )
      {
        if( !( auto_vest_mode ^ itr->auto_vest ) )
        {
          share_type to_deposit = fc::uint128_to_uint64( ( fc::uint128_t ( to_withdraw.value ) * itr->percent ) / HIVE_100_PERCENT );
          vests_deposited += to_deposit;

          if( to_deposit > 0 )
          {
            const auto& to_account = get< account_object, by_name >( itr->to_account );

            asset vests = asset( to_deposit, VESTS_SYMBOL );
            asset routed = auto_vest_mode ? vests : ( vests * cprops.get_vesting_share_price() );
            operation vop = fill_vesting_withdraw_operation( from_account.get_name(), to_account.get_name(), vests, routed );

            pre_push_virtual_operation( *this, vop );

            modify( to_account, [&]( account_object& a )
            {
              if( auto_vest_mode )
              {
                if( has_hardfork( HIVE_HARDFORK_0_20 ) )
                  rc().regenerate_rc_mana( a, now );
                a.vesting_shares += routed;
              }
              else
              {
                a.balance += routed;
              }
            });

            if( auto_vest_mode )
            {
              if( has_hardfork( HIVE_HARDFORK_0_20 ) )
                rc().update_account_after_vest_change( to_account, now );

              if( has_hardfork( HIVE_HARDFORK_1_24 ) )
              {
                FC_ASSERT( dv.valid(), "The object processing `delayed votes` must exist" );

                dv->add_votes( _votes_update_data_items,
                          to_account.get_id() == from_account.get_id()/*withdraw_executor*/,
                          routed.amount.value/*val*/,
                          to_account/*account*/
                        );
              }
              else
              {
                adjust_proxied_witness_votes( to_account, to_deposit );
              }
            }
            else
            {
              modify( cprops, [&]( dynamic_global_property_object& o )
              {
                o.total_vesting_fund_hive     -= routed;
                o.total_vesting_shares.amount -= to_deposit;
              });
            }

            post_push_virtual_operation( *this, vop );
          }
        }
      }
    };

    // Do two passes, the first for VESTS, the second for HIVE. Try to maintain as much accuracy for VESTS as possible.
    process_routing( true/*auto_vest_mode*/ );
    process_routing( false/*auto_vest_mode*/ );

    share_type to_convert = to_withdraw - vests_deposited;
    FC_ASSERT( to_convert >= 0, "Deposited more vests than were supposed to be withdrawn" );

    auto converted_hive = asset( to_convert, VESTS_SYMBOL ) * cprops.get_vesting_share_price();
    operation vop = fill_vesting_withdraw_operation( from_account.get_name(), from_account.get_name(), asset( to_convert, VESTS_SYMBOL ), converted_hive );
    //note: it has to be generated even if to_convert is zero because we've accumulated change on from_account
    //and only now we are going to update that account's VESTS (see issue #337)
    pre_push_virtual_operation( *this, vop );

    if( has_hardfork( HIVE_HARDFORK_0_20 ) )
      rc().regenerate_rc_mana( from_account, now );
    if( has_hardfork( HIVE_HARDFORK_1_24 ) )
    {
      FC_ASSERT( dv.valid() && "The object processing `delayed votes` must exist" );

      dv->add_votes( _votes_update_data_items,
                true/*withdraw_executor*/,
                -to_withdraw.value/*val*/,
                from_account/*account*/
              );
    }

    modify( from_account, [&]( account_object& a )
    {
      a.vesting_shares.amount -= to_withdraw;
      a.balance += converted_hive;
      a.withdrawn.amount += to_withdraw;

      if( a.get_total_vesting_withdrawal() <= 0 || a.get_vesting().amount == 0 )
      {
        a.vesting_withdraw_rate.amount = 0;
        a.next_vesting_withdrawal = fc::time_point_sec::maximum();
        a.to_withdraw.amount = 0;
        a.withdrawn.amount = 0;
      }
      else
      {
        a.next_vesting_withdrawal += fc::seconds( HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS );
      }
    });

    if( has_hardfork( HIVE_HARDFORK_0_20 ) )
      rc().update_account_after_vest_change( from_account, now, true, true );

    modify( cprops, [&]( dynamic_global_property_object& o )
    {
      o.total_vesting_fund_hive -= converted_hive;
      o.total_vesting_shares.amount -= to_convert;
    });

    if( has_hardfork( HIVE_HARDFORK_1_24 ) )
    {
      FC_ASSERT( dv.valid() && "Since HF24 the object processing `delayed votes` must exist" );

      fc::optional< ushare_type > leftover = dv->update_votes( _votes_update_data_items, now );
      FC_ASSERT( leftover.valid(), "Something went wrong" );
      if( leftover.valid() && ( *leftover ) > 0 )
        adjust_proxied_witness_votes( from_account, -( ( *leftover ).value ) );
    }
    else
    {
      if( to_withdraw > 0 )
        adjust_proxied_witness_votes( from_account, -to_withdraw );
    }

    post_push_virtual_operation( *this, vop );
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::withdraw_vesting_operation", count );
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::liquidity_reward_balance_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::hardfork_property_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::withdraw_vesting_route_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::liquidity_reward_balance_index>& chainbase::database::get_index<hive::chain::liquidity_reward_balance_index>() const;
template chainbase::generic_index<hive::chain::liquidity_reward_balance_index>& chainbase::database::get_mutable_index<hive::chain::liquidity_reward_balance_index>();

template const chainbase::generic_index<hive::chain::hardfork_property_index>& chainbase::database::get_index<hive::chain::hardfork_property_index>() const;
template chainbase::generic_index<hive::chain::hardfork_property_index>& chainbase::database::get_mutable_index<hive::chain::hardfork_property_index>();

template const chainbase::generic_index<hive::chain::withdraw_vesting_route_index>& chainbase::database::get_index<hive::chain::withdraw_vesting_route_index>() const;
template chainbase::generic_index<hive::chain::withdraw_vesting_route_index>& chainbase::database::get_mutable_index<hive::chain::withdraw_vesting_route_index>();

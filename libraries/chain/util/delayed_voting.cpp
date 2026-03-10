#include <hive/chain/util/delayed_voting.hpp>
#include <hive/chain/util/delayed_voting_processor.hpp>
#include <hive/chain/detail/state/account_details_object.hpp>
#include <hive/chain/detail/state/tiny_account_object.hpp>
#include <hive/chain/database_virtual_operations.hpp>

namespace hive { namespace chain {

void delayed_voting::add_delayed_value( const account_object& account, const time_point_sec& head_time, const ushare_type val )
{
  const auto& account_details = db.get_account_details( account.get_id() );
  db.modify( account_details, [&]( account_details_object& a )
  {
    delayed_voting_processor::add( a.get_delayed_votes(), a.get_sum_delayed_votes(), head_time, val );
  } );
  db.sync_tiny_delayed_votes( account, account_details );
}

void delayed_voting::erase_delayed_value( const account_object& account, const ushare_type val )
{
  const auto& account_details = db.get_account_details( account.get_id() );
  if( account_details.get_sum_delayed_votes() == 0 )
    return;

  db.modify( account_details, [&]( account_details_object& a )
  {
    delayed_voting_processor::erase( a.get_delayed_votes(), a.get_sum_delayed_votes(), val );
  } );
  db.sync_tiny_delayed_votes( account, account_details );
}

void delayed_voting::add_votes( opt_votes_update_data_items& items, const bool withdraw_executor, const share_type& val, const account_object& account )
{
  if( !items.valid() || val == 0 )
    return;

  votes_update_data vud { withdraw_executor, val, &account };

  auto found = items->find( vud );

  if( found == items->end() )
    items->emplace( vud );
  else
  {
    FC_ASSERT( found->withdraw_executor == withdraw_executor, "unexpected error: ${error}", ("error", delayed_voting_messages::incorrect_withdraw_data ) );
    found->val += val;
  }
}

fc::optional< ushare_type > delayed_voting::update_votes( const opt_votes_update_data_items& items, const time_point_sec& head_time )
{
  if( !items.valid() )
    return fc::optional< ushare_type >();

  ushare_type res{ 0ul };

  for( auto& item : *items )
  {
    FC_ASSERT(  ( !item.withdraw_executor && item.val > 0 ) ||
            ( item.withdraw_executor && item.val <= 0 ),
            "unexpected error: ${error}", ("error", delayed_voting_messages::incorrect_votes_update )
          );

    if( item.val == 0 )
      continue;

    if( item.val > 0 )
      add_delayed_value( *item.account, head_time, item.val.value );
    else
    {
      const ushare_type abs_val{ static_cast< ushare_type >( -item.val.value ) };
      const auto& account_details = db.get_account_details( item.account->get_id() );
      if( abs_val >= account_details.get_sum_delayed_votes() )
      {
        res = abs_val - account_details.get_sum_delayed_votes();
        erase_delayed_value( *item.account, account_details.get_sum_delayed_votes() );
      }
      else
        erase_delayed_value( *item.account, abs_val );
    }
  }

  return res;
}

void delayed_voting::run( const fc::time_point_sec& head_time )
{
  const auto& idx = db.get_index< tiny_account_index, by_delayed_voting >();
  auto current = idx.begin();

  int count = 0;
  if( db.get_benchmark_dumper().is_enabled() )
    db.get_benchmark_dumper().begin();
  while( current != idx.end() && current->has_delayed_votes() &&
        head_time >= ( current->get_oldest_delayed_vote_time() + HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS )
      )
  {
    // Look up the account_details_object for this account to get the actual delayed vote data
    const auto& account_details = db.get_account_details( account_id_type( account_object::id_type( current->get_id().get_value() ) ) );
    const ushare_type _val{ account_details.get_delayed_votes().begin()->val };

    // Get the account_object
    const auto& account = db.get_account( current->get_name() );

    //dlog( "account: ${acc} delayed_votes: ${dv} time: ${time}", ( "acc", account.get_name() )( "dv", _val )( "time", account_details.get_delayed_votes().begin()->time.to_iso_string() ) );

    operation vop = hive::protocol::delayed_voting_operation( account.get_name(), _val );
    /// Push vop to be recorded by other parts (like AH plugin etc.)
    push_virtual_operation( db, vop );

    db.adjust_proxied_witness_votes( account, _val.value );

    /*
      The operation `transfer_to_vesting` always adds elements to `delayed_votes` collection in `account_details_object`.
      In terms of performance is necessary to hold size of `delayed_votes` not greater than `30`.

      Why `30`? HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS / HIVE_DELAYED_VOTING_INTERVAL_SECONDS == 30

      Solution:
        The best solution is to add new record at the back and to remove at the front.
    */
    db.modify( account_details, [&]( account_details_object& a )
    {
      delayed_voting_processor::erase_front( a.get_delayed_votes(), a.get_sum_delayed_votes() );
    } );
    // Sync tiny_account_object with delayed_votes changes
    db.modify( *current, [&]( tiny_account_object& t )
    {
      t.modify_from_delayed_votes( account_details );
    } );

    current = idx.begin();
    ++count;
  }
  if( db.get_benchmark_dumper().is_enabled() && count )
    db.get_benchmark_dumper().end( "processing", "hive::protocol::transfer_to_vesting_operation", count );
}

} } // namespace hive::chain

#include <steem/chain/util/delayed_voting.hpp>
#include <steem/chain/util/delayed_voting_processor.hpp>

namespace steem { namespace chain {

using steem::protocol::asset;

void delayed_voting::add_delayed_value( const account_object& account, const time_point_sec& head_time, uint64_t val )
{
   db.modify( account, [&]( account_object& a )
   {
      delayed_voting_processor::add( a.delayed_votes, a.sum_delayed_votes, head_time, val );
   } );
}

void delayed_voting::erase_delayed_value( const account_object& account, uint64_t val )
{
   db.modify( account, [&]( account_object& a )
   {
      delayed_voting_processor::erase( a.delayed_votes, a.sum_delayed_votes, val );
   } );
}

void delayed_voting::add_votes( opt_votes_update_data_items& items, bool withdraw_executer, int64_t val, const account_object& account )
{
   if( !items.valid() )
      return;

   votes_update_data vud { withdraw_executer, val, &account };

   auto found = items->find( vud );

   if( found == items->end() )
      items->emplace( vud );
   else
      found->val += val;
}

fc::optional< uint64_t > delayed_voting::update_votes( const opt_votes_update_data_items& items, const time_point_sec& head_time )
{
   if( !items.valid() )
      return fc::optional< uint64_t >();

   uint64_t res = 0;

   for( auto& item : *items )
   {
      FC_ASSERT(  ( !item.withdraw_executer && item.val > 0 ) ||
                  ( item.withdraw_executer && item.val <= 0 ),
                  "unexpected error: ${error}", ("error", delayed_voting_messages::incorrect_votes_update )
               );

      if( item.val == 0 )
         continue;

      if( item.val > 0 )
         add_delayed_value( *item.account, head_time, item.val );
      else
      {
         uint64_t abs_val = std::abs( item.val );
         if( abs_val >= item.account->sum_delayed_votes )
         {
            res = abs_val - item.account->sum_delayed_votes;
            if( item.account->sum_delayed_votes > 0 )
               erase_delayed_value( *item.account, item.account->sum_delayed_votes );
         }
         else
            erase_delayed_value( *item.account, abs_val );
      }
   }

   return res;
}

//Question: To push virtual operation? What operation? What for?
void delayed_voting::run( const block_notification& note )
{
   auto head_time = note.block.timestamp;

   const auto& idx = db.get_index< account_index, by_delayed_voting >();
   auto current = idx.begin();

   while( current != idx.end() &&
          current->get_the_earliest_time() != time_point_sec::maximum() &&
          head_time >= ( current->get_the_earliest_time() + STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS )
        )
   {
      int64_t _val = current->delayed_votes.begin()->val;

      db.adjust_proxied_witness_votes( *current, _val );

      /*
         The operation `transfer_to_vesting` always adds elements to `delayed_votes` collection in `account_object`.
         In terms of performance is necessary to hold size of `delayed_votes` not greater than `30`.

         Why `30`? STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS / STEEM_DELAYED_VOTING_INTERVAL_SECONDS == 30

         Solution:
            The best solution is to add new record at the back and to remove at the front.
      */
      db.modify( *current, [&]( account_object& a )
      {
         delayed_voting_processor::erase_front( a.delayed_votes, a.sum_delayed_votes );
      } );

      current = idx.begin();
   }
}

} } // namespace steem::chain

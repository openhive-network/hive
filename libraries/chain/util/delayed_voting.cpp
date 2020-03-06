#include <steem/chain/util/delayed_voting.hpp>
#include <steem/chain/util/delayed_voting_processor.hpp>

namespace steem { namespace chain {

using steem::protocol::asset;

void delayed_voting::save_delayed_value( const account_object& account, const time_point_sec& head_time, int64_t val )
{
   db.modify( account, [&]( account_object& a )
   {
      delayed_voting_processor::add( a.delayed_votes, a.sum_delayed_votes, head_time, val );
   } );
}

//Question: To push virtual operation? What operation? What for?
void delayed_voting::run( const block_notification& note )
{
   auto head_time = note.block.timestamp;

   const auto& idx = db.get_index< account_index, by_delayed_voting >();
   auto current = idx.begin();

   while( current != idx.end() &&
          current->get_the_earliest_time() != time_point_sec::maximum() &&
          head_time > ( current->get_the_earliest_time() + STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS )
        )
   {
      int64_t _val = current->delayed_votes.begin()->val;

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

      if( current->vesting_shares.amount.value > _val )
         db.adjust_proxied_witness_votes( *current, _val );

      ++current;
   }
}

} } // namespace steem::chain

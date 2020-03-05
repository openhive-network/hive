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

void delayed_voting::run( const block_notification& note )
{
   auto head_time = note.block.timestamp;

   const auto& idx = db.get_index< account_index, by_delayed_voting >();
   auto current = idx.begin();

   while( current != idx.end() && ( head_time > ( current->get_the_earliest_time() + STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS ) ) )
   {
      int64_t delayed_votes_sum = 0;

      while(
               !current->delayed_votes.empty() &&
               ( head_time > ( current->delayed_votes.begin()->time + STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS ) )
           )
      {
         delayed_votes_sum += current->delayed_votes.begin()->val;

         db.modify( *current, [&]( account_object& a )
         {
            delayed_voting_processor::erase_front( a.delayed_votes );
         } );
         //Question: To push virtual operation? What operation? What for?
      }

      if( current->vesting_shares.amount >= delayed_votes_sum )
         db.adjust_proxied_witness_votes< true/*ALLOW_VOTE*/ >( *current, delayed_votes_sum );
      else
         db.adjust_proxied_witness_votes< true/*ALLOW_VOTE*/ >( *current, current->vesting_shares.amount );
   }
}

} } // namespace steem::chain

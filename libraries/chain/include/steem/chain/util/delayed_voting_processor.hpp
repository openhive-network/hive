#pragma once

namespace steem { namespace chain {

struct delayed_votes_data
{
   time_point_sec    time;
   int64_t           val = 0;
};

struct delayed_voting_processor
{
   template< typename COLLECTION_TYPE >
   static void add( COLLECTION_TYPE& items, int64_t& sum, const time_point_sec& head_time, int64_t val )
   {
      /*
         A collection is filled gradually - every item in `items` is created each STEEM_DELAY_VOTING_INTERVAL_SECONDS time.

         Input data:
            2020-03-10 04:00:00 1000
            2020-03-10 05:00:00 2000
            2020-03-10 11:00:00 7000
            2020-03-11 01:00:00 6000
            2020-03-12 02:00:00 8000
            2020-03-12 02:00:00 200000

         Result:
            items[0] = {2020-03-10,  10000}
            items[1] = {2020-03-11,  6000}
            items[2] = {2020-03-12,  208000}
      */
      sum += val;

      if( items.empty() )
      {
         items.emplace_back( delayed_votes_data{ head_time, val } );
         return;
      }

      auto front_obj = items.front();
      FC_ASSERT( front_obj.time >= head_time, "head date must be greater or equals to last delayed voting data" );
      if( head_time > front_obj.time + STEEM_DELAYED_VOTING_INTERVAL_SECONDS )
      {
         items.emplace_back( delayed_votes_data{ head_time, val } );
      }
      else
      {
         front_obj.val += val;
      }
   }

   template< typename COLLECTION_TYPE >
   static void erase_front( COLLECTION_TYPE& items )
   {
      items.erase( items.begin() );
   }

};

} } // namespace steem::chain

FC_REFLECT( steem::chain::delayed_votes_data,
             (time)(val)
          )

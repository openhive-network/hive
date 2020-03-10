#pragma once

#include <chainbase/allocators.hpp>

#include <steem/chain/database.hpp>
#include <steem/chain/index.hpp>
#include <steem/chain/account_object.hpp>

namespace steem { namespace chain {

class delayed_voting
{
   private:

      chain::database& db;

   public:

      delayed_voting( chain::database& _db ) : db( _db ){}

      void save_delayed_value( const account_object& account, const time_point_sec& head_time, int64_t val );
      void erase_delayed_value( const account_object& account, int64_t val );

      void run( const block_notification& note );
};

} } // namespace steem::chain

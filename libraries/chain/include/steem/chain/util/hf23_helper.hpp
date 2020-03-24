#pragma once

#include <fc/time.hpp>
#include <fc/variant.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>

#include <steem/chain/database.hpp>
#include <steem/chain/account_object.hpp>

#include <steem/protocol/asset.hpp>

#include<string>

namespace steem { namespace chain {

class hf23_helper
{
   public:

      struct hf23_item
      {
         std::string name;

         asset       balance;
         asset       savings_balance;
         asset       sbd_balance;
         asset       savings_sbd_balance;
         asset       reward_sbd_balance;
         asset       reward_steem_balance;

         bool operator == ( const hf23_item& obj ) const
         {
            return   balance == obj.balance &&
                     savings_balance == obj.savings_balance &&
                     sbd_balance == obj.sbd_balance &&
                     savings_sbd_balance == obj.savings_sbd_balance &&
                     reward_sbd_balance == obj.reward_sbd_balance &&
                     reward_steem_balance == obj.reward_steem_balance;
         }

         static hf23_item get_balances( const account_object& obj )
         {
            hf23_item res;

            res.name                   = obj.name;

            res.balance                = obj.balance;
            res.savings_balance        = obj.savings_balance;
            res.sbd_balance            = obj.sbd_balance;
            res.savings_sbd_balance    = obj.savings_sbd_balance;
            res.reward_sbd_balance     = obj.reward_sbd_balance;
            res.reward_steem_balance   = obj.reward_steem_balance;

            return res;
         }

      };

      using hf23_items = std::vector< hf23_item >;

   private:

      template< typename T = asset >
      static T from_variant_object( const std::string& field_name, const fc::variant_object& obj );

      static fc::variant to_variant( const account_object& account );
      static hf23_item from_variant( const fc::variant& obj );

   public:

      static void dump_balances( const database& db, const std::string& path, const std::set< std::string >& accounts );
      static hf23_items restore_balances( const std::string& path );
};

} } // namespace steem::chain

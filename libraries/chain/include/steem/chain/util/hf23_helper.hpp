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
         asset       sbd_balance;

         bool operator == ( const hf23_item& obj ) const
         {
            return balance == obj.balance && sbd_balance == obj.sbd_balance;
         }

         static hf23_item get_balances( const account_object& obj )
         {
            hf23_item res;

            res.name                   = obj.name;

            res.balance                = obj.balance;
            res.sbd_balance            = obj.sbd_balance;

            return res;
         }

      };

      using hf23_items = std::vector< hf23_item >;

   private:

      template< typename T = asset >
      static T from_variant_object( const std::string& field_name, const fc::variant_object& obj );

      static fc::variant to_variant( const hf23_item& item );
      static hf23_item from_variant( const fc::variant& obj );

   public:

      static void gather_balance( hf23_items& source, const std::string& name, const asset& balance, const asset& sbd_balance );
      static void dump_balances( const std::string& path, const hf23_items& source );
      static hf23_items restore_balances( const std::string& path );
};

} } // namespace steem::chain

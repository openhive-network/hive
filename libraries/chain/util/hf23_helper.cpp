#include <steem/chain/util/hf23_helper.hpp>

namespace steem { namespace chain {

fc::variant hf23_helper::to_variant( const account_object& account )
{
   return fc::mutable_variant_object
      ( "name",                  account.name )
      ( "balance",               account.balance )
      ( "savings_balance",       account.savings_balance )
      ( "sbd_balance",           account.sbd_balance )
      ( "savings_sbd_balance",   account.savings_sbd_balance )
      ( "reward_sbd_balance",    account.reward_sbd_balance )
      ( "reward_steem_balance",  account.reward_steem_balance );
}

template< typename T >
T hf23_helper::from_variant_object( const std::string& field_name, const fc::variant_object& obj )
{
   if( obj.contains( field_name.c_str() ) )
      return obj[ field_name ].as< T >();

   std::string message = "Lack of `" + field_name + "` field";
   throw std::runtime_error( message );
}

hf23_helper::hf23_item hf23_helper::from_variant( const fc::variant& obj )
{
   hf23_item res;

   if( !obj.is_object() )
      throw std::runtime_error( "Object doesn't exist" );

   auto _obj = obj.get_object();

   res.name =                 from_variant_object< std::string >( "name", _obj );
   res.balance =              from_variant_object( "balance", _obj );
   res.savings_balance =      from_variant_object( "savings_balance", _obj );
   res.sbd_balance =          from_variant_object( "sbd_balance", _obj );
   res.savings_sbd_balance =  from_variant_object( "savings_sbd_balance", _obj );
   res.reward_sbd_balance =   from_variant_object( "reward_sbd_balance", _obj );
   res.reward_steem_balance = from_variant_object( "reward_steem_balance", _obj );

   return res;
}

void hf23_helper::dump_balances( const database& db, const std::string& path, const std::set< std::string >& accounts )
{
   const fc::path _path( path.c_str() );

   try
   {
      fc::variants _accounts;
      
      for( auto& account_name : accounts )
      {
            const auto* account_ptr = db.find_account( account_name );
            if( account_ptr == nullptr )
               continue;

         auto v = to_variant( *account_ptr );
         _accounts.emplace_back( v );
      }

      fc::json::save_to_file( _accounts, _path );
   }
   catch ( const std::exception& e )
   {
      elog( "Error while writing dump data to file: ${e}", ("e", e.what()) );
   }
   catch ( const fc::exception& e )
   {
      elog( "Error while writing dump data to file ${filename}: ${e}",
            ( "filename", path )("e", e.to_detail_string() ) );
   }
   catch (...)
   {
      elog( "Error while writing dump data to file ${filename}",
            ( "filename", path ) );
   }
}

hf23_helper::hf23_items hf23_helper::restore_balances( const std::string& path )
{
   hf23_items res;

   try
   {
      const fc::path _path( path.c_str() );
      fc::variant obj = fc::json::from_file( _path, fc::json::strict_parser );

      if( obj.is_array() )
      {
         const fc::variants& items = obj.get_array();
         for( auto& item : items )
            res.emplace_back( from_variant( item ) );
      }
   }
   catch ( const std::exception& e )
   {
      elog( "Error while dump reading: ${e}", ("e", e.what()) );
   }
   catch ( const fc::exception& e )
   {
      elog( "Error while dump reading: ${e}", ("e", e.what()) );
   }
   catch (...)
   {
      elog( "Error while writing dump data to file ${filename}",
            ( "filename", path ) );
   }

   return res;
}

} } // namespace steem::chain


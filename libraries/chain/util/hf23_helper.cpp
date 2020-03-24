#include <steem/chain/util/hf23_helper.hpp>

namespace steem { namespace chain {

fc::variant hf23_helper::to_variant( const hf23_item& item )
{
   return fc::mutable_variant_object
      ( "name",                  item.name )
      ( "balance",               item.balance )
      ( "sbd_balance",           item.sbd_balance );
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
   res.sbd_balance =          from_variant_object( "sbd_balance", _obj );

   return res;
}

void hf23_helper::gather_balance( hf23_items& source, const std::string& name, const asset& balance, const asset& sbd_balance )
{
   source.emplace_back( hf23_item{ name, balance, sbd_balance } );
}

void hf23_helper::dump_balances( const std::string& path, const hf23_items& source )
{
   const fc::path _path( path.c_str() );

   try
   {
      fc::variants _accounts;
      
      for( auto& item : source )
      {
         auto v = to_variant( item );
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


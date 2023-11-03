#include <core/utilities.hpp>

#include <boost/algorithm/string.hpp>

namespace beekeeper {

  bool public_key_details::operator<( const public_key_details& obj ) const
  {
    return public_key < obj.public_key;
  }

  std::vector< std::string > list_all_wallets( const boost::filesystem::path& directory, const std::string& extension )
  {
    std::vector< std::string > _result;

    boost::filesystem::directory_iterator _end_itr;
    for( boost::filesystem::directory_iterator itr( directory ); itr != _end_itr; ++itr )
    {
      if( boost::filesystem::is_regular_file( itr->path() ) )
      {
        if( itr->path().extension() == extension )
        {
          std::vector<std::string> _path_parts;
          boost::split( _path_parts, itr->path().c_str(), boost::is_any_of("/") );
          if( !_path_parts.empty() )
          {
            std::vector<std::string> _file_parts;
            boost::split( _file_parts, *_path_parts.rbegin(), boost::is_any_of(".") );
            if( !_file_parts.empty() )
              _result.emplace_back( *_file_parts.begin() );
          }
        }
      }
    }
    return _result;
  }

}
namespace fc
{
  void to_variant( const beekeeper::wallet_details& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "name", var.name )( "unlocked", var.unlocked );
    vo = v;
  }

  void to_variant( const beekeeper::get_info_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "now", var.now )( "timeout_time", var.timeout_time );
    vo = v;
  }

  void to_variant( const beekeeper::create_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "password", var.password );
    vo = v;
  }

  void to_variant( const beekeeper::import_key_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "public_key", var.public_key );
    vo = v;
  }

  void to_variant( const beekeeper::create_session_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "token", var.token );
    vo = v;
  }
  
  void to_variant( const beekeeper::get_public_keys_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "keys", var.keys );
    vo = v;
  }

  void to_variant( const beekeeper::list_wallets_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "wallets", var.wallets );
    vo = v;
  }

  void to_variant( const beekeeper::public_key_details& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "public_key", var.public_key );
    vo = v;
  }

  void to_variant( const beekeeper::signature_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "signature", var.signature );
    vo = v;
  }

  void to_variant( const beekeeper::init_data& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "status", var.status )( "version", var.version );
    vo = v;
  }

  void to_variant( const beekeeper::has_matching_private_key_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "exists", var.exists );
    vo = v;
  }
} // fc

#include <core/utilities.hpp>

#include <fc/git_revision.hpp>

#include <boost/algorithm/string.hpp>

namespace beekeeper {

  bool public_key_details::operator<( const public_key_details& obj ) const
  {
    return public_key < obj.public_key;
  }

  namespace utility
  {
    std::string get_revision()
    {
      return fc::git_revision_sha;
    }
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

  void to_variant( const beekeeper::import_keys_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "public_keys", var.public_keys );
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

  void to_variant( const beekeeper::encrypt_data_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "encrypted_content", var.encrypted_content );
    vo = v;
  }

  void to_variant( const beekeeper::decrypt_data_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "decrypted_content", var.decrypted_content );
    vo = v;
  }

  void to_variant( const beekeeper::get_version_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "version", var.version );
    vo = v;
  }

  void from_variant( const fc::variant& var,  beekeeper::wallet_data& vo )
  {
   from_variant( var, vo.cipher_keys );
  }

  void to_variant( const beekeeper::wallet_data& var, fc::variant& vo )
  {
    to_variant( var.cipher_keys, vo );
  }

  void to_variant( const beekeeper::has_wallet_return& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "exists", var.exists );
    vo = v;
  }
} // fc

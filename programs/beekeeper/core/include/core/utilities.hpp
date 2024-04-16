#pragma once

#include <fc/reflect/reflect.hpp>
#include <fc/container/flat.hpp>
#include <fc/container/flat_fwd.hpp>
#include <fc/crypto/elliptic.hpp>

#include <functional>
#include <string>
#include <optional>
#include <chrono>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/filesystem.hpp>

namespace beekeeper {

using private_key_type  = fc::ecc::private_key;
using public_key_type   = fc::ecc::public_key;
using signature_type    = fc::ecc::compact_signature;
using digest_type       = fc::sha256;
using chain_id_type     = fc::sha256;

using fc::flat_set;

using seconds_type = uint32_t;

struct init_data
{
  bool status = false;
  std::string version;
};

struct wallet_details
{
  std::string name;
  bool unlocked = false;
};

struct public_key_details
{
  std::string public_key;
  bool operator<( const public_key_details& obj ) const ;
};

struct info
{
  std::string now;
  std::string timeout_time;
};

namespace types
{
  using basic_method_type         = std::function<void( const std::string& )>;
  using notification_method_type  = basic_method_type;
  using lock_method_type          = basic_method_type;
  using timepoint_t               = std::chrono::time_point<std::chrono::system_clock>;
}

namespace utility
{
  namespace public_key
  {
    inline public_key_type create( const std::string& source )
    {
      return public_key_type::from_base58( source, false/*is_sha256*/ );
    }

    inline std::string to_string( const public_key_type& source )
    {
      return public_key_type::to_base58( source, false/*is_sha256*/ );
    }
  }

  template<typename source_type>
  flat_set<public_key_details> get_public_keys( const source_type& source )
  {
    flat_set<public_key_details> _result;

    std::transform( source.begin(), source.end(), std::inserter( _result, _result.end() ),
    []( const public_key_type& public_key ){ return public_key_details{ public_key::to_string( public_key ) }; } );

    return _result;
  }
}

struct session_token_type
{
  std::string token;
};

struct wallet_args: public session_token_type
{
  std::string wallet_name;
};
struct wallet_password_args: public session_token_type
{
  std::string wallet_name;
  std::string password;
};

struct create_args: public session_token_type
{
  std::string wallet_name;
  std::optional<std::string> password;
};
struct create_return
{
  std::string password;
};

struct void_type {};

using open_args   = wallet_args;
using open_return = void_type;

using close_args   = wallet_args;
using close_return = void_type;

struct set_timeout_args: public session_token_type
{
  seconds_type seconds;
};
using set_timeout_return = void_type;

using lock_all_args   = session_token_type;
using lock_all_return = void_type;

using lock_args   = wallet_args;
using lock_return = void_type;

using unlock_args   = wallet_password_args;
using unlock_return = void_type;

struct remove_key_args: public session_token_type
{
  std::string wallet_name;
  std::string public_key;
};
using remove_key_return = void_type;

struct import_key_return
{
  std::string public_key;
};

struct import_key_args: public session_token_type
{
  std::string wallet_name;
  std::string wif_key;
};

using list_wallets_args = session_token_type;
struct list_wallets_return
{
  std::vector<wallet_details> wallets;
};

using list_created_wallets_args = list_wallets_args;
using list_created_wallets_return = list_wallets_return;

struct get_public_keys_args: public session_token_type
{
  std::optional<std::string> wallet_name;
};
struct get_public_keys_return
{
  flat_set<public_key_details> keys;
};

struct signature_return
{
  signature_type signature;
};
struct sign_digest_args: public session_token_type
{
  std::string sig_digest;
  std::string public_key;
  std::optional<std::string> wallet_name;
};
using sign_digest_return = signature_return;

using get_info_args   = session_token_type;
using get_info_return = info;

struct create_session_args
{
  std::optional<std::string> salt;
  std::optional<std::string> notifications_endpoint;
};
using create_session_return = session_token_type;
using close_session_args = session_token_type;
using close_session_return = void_type;

struct has_matching_private_key_args : public wallet_args
{
  std::string public_key;
};

struct has_matching_private_key_return
{
  bool exists = false;
};

struct encrypt_data_args : public wallet_args
{
  std::string from_public_key;
  std::string to_public_key;
  std::string content;
  std::optional<unsigned int> nonce;
};
struct encrypt_data_return
{
  std::string encrypted_content;
};

struct decrypt_data_args : public wallet_args
{
  std::string from_public_key;
  std::string to_public_key;
  std::string encrypted_content;
};
struct decrypt_data_return
{
  std::string decrypted_content;
};
struct exception
{
  static std::pair<std::string, bool> exception_handler( std::function<std::string()>&& method, const std::string& additional_message = "" )
  {
    std::string _error_message = additional_message;

    try
    {
      return { method(), true };
    }
    catch (const boost::exception& e)
    {
      _error_message += boost::diagnostic_information(e);
    }
    catch (const fc::exception& e)
    {
      _error_message += e.to_detail_string();
    }
    catch (const std::exception& e)
    {
      _error_message += e.what();
    }
    catch (...)
    {
      _error_message += "unknown exception";
    }
    elog( _error_message );

    return { _error_message, false };
  }
};

}
namespace fc
{
  void to_variant( const beekeeper::wallet_details& var, fc::variant& vo );
  void to_variant( const beekeeper::get_info_return& var, fc::variant& vo );
  void to_variant( const beekeeper::create_return& var, fc::variant& vo );
  void to_variant( const beekeeper::import_key_return& var, fc::variant& vo );
  void to_variant( const beekeeper::create_session_return& var, fc::variant& vo );
  void to_variant( const beekeeper::get_public_keys_return& var, fc::variant& vo );
  void to_variant( const beekeeper::list_wallets_return& var, fc::variant& vo );
  void to_variant( const beekeeper::public_key_details& var, fc::variant& vo );
  void to_variant( const beekeeper::signature_return& var, fc::variant& vo );
  void to_variant( const beekeeper::init_data& var, fc::variant& vo );
  void to_variant( const beekeeper::has_matching_private_key_return& var, fc::variant& vo );
  void to_variant( const beekeeper::encrypt_data_return& var, fc::variant& vo );
  void to_variant( const beekeeper::decrypt_data_return& var, fc::variant& vo );
}

FC_REFLECT( beekeeper::init_data, (status)(version) )

FC_REFLECT( beekeeper::wallet_details, (name)(unlocked) )
FC_REFLECT( beekeeper::public_key_details, (public_key) )
FC_REFLECT( beekeeper::info, (now)(timeout_time) )

FC_REFLECT( beekeeper::void_type, )
FC_REFLECT( beekeeper::session_token_type, (token) )
FC_REFLECT_DERIVED( beekeeper::wallet_args, (beekeeper::session_token_type), (wallet_name) )
FC_REFLECT_DERIVED( beekeeper::wallet_password_args, (beekeeper::session_token_type), (wallet_name)(password) )
FC_REFLECT_DERIVED( beekeeper::create_args, (beekeeper::session_token_type), (wallet_name)(password) )
FC_REFLECT( beekeeper::create_return, (password) )
FC_REFLECT_DERIVED( beekeeper::set_timeout_args, (beekeeper::session_token_type), (seconds) )
FC_REFLECT_DERIVED( beekeeper::import_key_args, (beekeeper::session_token_type), (wallet_name)(wif_key) )
FC_REFLECT_DERIVED( beekeeper::remove_key_args, (beekeeper::session_token_type), (wallet_name)(public_key) )
FC_REFLECT( beekeeper::import_key_return, (public_key) )
FC_REFLECT( beekeeper::list_wallets_return, (wallets) )
FC_REFLECT( beekeeper::get_public_keys_return, (keys) )
FC_REFLECT_DERIVED( beekeeper::sign_digest_args, (beekeeper::session_token_type), (sig_digest)(public_key)(wallet_name) )
FC_REFLECT( beekeeper::signature_return, (signature) )
FC_REFLECT( beekeeper::create_session_args, (salt)(notifications_endpoint) )
FC_REFLECT_DERIVED( beekeeper::get_public_keys_args, (beekeeper::session_token_type), (wallet_name) )
FC_REFLECT_DERIVED( beekeeper::has_matching_private_key_args, (beekeeper::wallet_args), (public_key) )
FC_REFLECT( beekeeper::has_matching_private_key_return, (exists) )
FC_REFLECT_DERIVED( beekeeper::encrypt_data_args, (beekeeper::wallet_args), (from_public_key)(to_public_key)(content)(nonce) )
FC_REFLECT( beekeeper::encrypt_data_return, (encrypted_content) )
FC_REFLECT_DERIVED( beekeeper::decrypt_data_args, (beekeeper::wallet_args), (from_public_key)(to_public_key)(encrypted_content) )
FC_REFLECT( beekeeper::decrypt_data_return, (decrypted_content) )
#pragma once

#include <core/beekeeper_wallet_base.hpp>

#include <fc/reflect/reflect.hpp>
#include <fc/container/flat.hpp>

#include <functional>
#include <string>
#include <chrono>

#include <boost/exception/diagnostic_information.hpp>

namespace beekeeper {

using seconds_type = uint32_t;

struct init_data
{
  bool status = false;
  std::string token;
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
  using basic_method_type         = std::function<void()>;
  using notification_method_type  = basic_method_type;
  using lock_method_type          = basic_method_type;
  using timepoint_t               = std::chrono::time_point<std::chrono::system_clock>;
}

namespace utility
{
  inline public_key_type get_public_key( const std::string& source )
  {
    return public_key_type::from_base58( source, false/*is_sha256*/ );
  }

  template<typename source_type>
  flat_set<public_key_details> get_public_keys( const source_type& source )
  {
    flat_set<public_key_details> _result;

    std::transform( source.begin(), source.end(), std::inserter( _result, _result.end() ),
    []( const public_key_type& public_key ){ return public_key_details{ public_key_type::to_base58( public_key, false/*is_sha256*/ ) }; } );

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
  fc::optional<std::string> password{};
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
  std::string password;
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

using get_public_keys_args = session_token_type;
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
  std::string public_key;
  std::string sig_digest;
};
using sign_digest_return = signature_return;

struct sign_transaction_args: public session_token_type
{
  std::string transaction;
  chain_id_type chain_id;
  std::string public_key;
};
using sign_transaction_return = signature_return;

using sign_binary_transaction_args = sign_transaction_args;
using sign_binary_transaction_return = sign_transaction_return;

using get_info_args   = session_token_type;
using get_info_return = info;

struct create_session_args
{
  std::string salt;
  std::string notifications_endpoint;
};
using create_session_return = session_token_type;
using close_session_args = session_token_type;
using close_session_return = void_type;

struct exception
{
  static std::pair<std::string, bool> exception_handler( std::function<std::string()>&& method )
  {
    std::string _error_message = "";

    try
    {
      return { method(), true };
    }
    catch (const boost::exception& e)
    {
      _error_message = boost::diagnostic_information(e);
    }
    catch (const fc::exception& e)
    {
      _error_message = e.to_detail_string();
    }
    catch (const std::exception& e)
    {
      _error_message = e.what();
    }
    catch (...)
    {
      _error_message = "unknown exception";
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
}

FC_REFLECT( beekeeper::init_data, (status)(token) )

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
FC_REFLECT_DERIVED( beekeeper::remove_key_args, (beekeeper::session_token_type), (wallet_name)(password)(public_key) )
FC_REFLECT( beekeeper::import_key_return, (public_key) )
FC_REFLECT( beekeeper::list_wallets_return, (wallets) )
FC_REFLECT( beekeeper::get_public_keys_return, (keys) )
FC_REFLECT_DERIVED( beekeeper::sign_digest_args, (beekeeper::session_token_type), (public_key)(sig_digest) )
FC_REFLECT( beekeeper::signature_return, (signature) )
FC_REFLECT_DERIVED( beekeeper::sign_transaction_args, (beekeeper::session_token_type), (transaction)(chain_id)(public_key) )
FC_REFLECT( beekeeper::create_session_args, (salt)(notifications_endpoint) )

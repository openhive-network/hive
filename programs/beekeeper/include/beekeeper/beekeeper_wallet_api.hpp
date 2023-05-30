#pragma once
#include <beekeeper/beekeeper_wallet_manager.hpp>

#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/protocol/types.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>

namespace beekeeper {

namespace detail
{
  class beekeeper_api_impl;
}

using hive::plugins::json_rpc::void_type;
using beekeeper::signature_type;

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
struct create_return: public session_token_type
{
  std::string password;
};

using open_args   = wallet_args;
using open_return = void_type;

using close_args   = wallet_args;
using close_return = void_type;

struct set_timeout_args: public session_token_type
{
  int64_t seconds;
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
  flat_set<std::string> keys;
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

struct sign_transaction_args: public sign_digest_args
{
  std::string transaction;
  chain_id_type chain_id;
};
using sign_transaction_return = signature_return;

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

class beekeeper_wallet_api
{
  public:
    beekeeper_wallet_api( std::shared_ptr<beekeeper::beekeeper_wallet_manager> wallet_mgr );
    ~beekeeper_wallet_api();

    DECLARE_API(
      (create)
      (open)
      (close)
      (set_timeout)
      (lock_all)
      (lock)
      (unlock)
      (import_key)
      (remove_key)
      (list_wallets)
      (get_public_keys)
      (sign_digest)
      (sign_transaction)
      (get_info)
      (create_session)
      (close_session)
    )

  private:
    std::unique_ptr< detail::beekeeper_api_impl > my;
};

} // beekeeper

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
FC_REFLECT_DERIVED( beekeeper::sign_transaction_args, (beekeeper::sign_digest_args), (transaction)(chain_id) )
FC_REFLECT( beekeeper::create_session_args, (salt)(notifications_endpoint) )

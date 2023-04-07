#pragma once
#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/protocol/types.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>

namespace hive { namespace plugins { namespace clive_api {

namespace detail
{
  class clive_api_impl;
}

using plugins::json_rpc::void_type;
using hive::plugins::clive::public_key_type;
using hive::plugins::clive::private_key_type;

struct wallet_args
{
  std::string wallet_name;
};
struct wallet_password_args
{
  std::string wallet_name;
  std::string password;
};

using create_args = wallet_args;
struct create_return
{
  std::string password;
};

using open_args   = wallet_args;
using open_return = void_type;

struct set_timeout_args
{
  int64_t seconds;
};
using set_timeout_return = void_type;

using lock_all_args   = void_type;
using lock_all_return = void_type;

using lock_args   = wallet_args;
using lock_return = void_type;

using unlock_args   = wallet_password_args;
using unlock_return = void_type;

struct import_key_args
{
  std::string wallet_name;
  std::string wif_key;
};
using import_key_return = void_type;

struct remove_key_args
{
  std::string wallet_name;
  std::string password;
  std::string public_key;
};
using remove_key_return = void_type;

using create_key_args   = wallet_args;
struct create_key_return
{
  std::string public_key;
};

using list_wallets_args = void_type;
struct list_wallets_return
{
  std::vector<std::string> wallets;
};

using list_keys_args = wallet_password_args;
struct list_keys_return
{
  map<std::string, std::string> keys;
};

using get_public_keys_args = void_type;
struct get_public_keys_return
{
  flat_set<std::string> keys;
};

class clive_api
{
  public:
    clive_api();
    ~clive_api();

    DECLARE_API(
      (create)
      (open)
      (set_timeout)
      (lock_all)
      (lock)
      (unlock)
      (import_key)
      (remove_key)
      (create_key)
      (list_wallets)
      (list_keys)
      (get_public_keys)
    )

  private:
    std::unique_ptr< detail::clive_api_impl > my;
};

} } } // hive::plugins::clive_api

FC_REFLECT( hive::plugins::clive_api::wallet_args, (wallet_name) )
FC_REFLECT( hive::plugins::clive_api::wallet_password_args, (wallet_name)(password) )
FC_REFLECT( hive::plugins::clive_api::create_return, (password) )
FC_REFLECT( hive::plugins::clive_api::set_timeout_args, (seconds) )
FC_REFLECT( hive::plugins::clive_api::import_key_args, (wallet_name)(wif_key) )
FC_REFLECT( hive::plugins::clive_api::remove_key_args, (wallet_name)(password)(public_key) )
FC_REFLECT( hive::plugins::clive_api::create_key_return, (public_key) )
FC_REFLECT( hive::plugins::clive_api::list_wallets_return, (wallets) )
FC_REFLECT( hive::plugins::clive_api::list_keys_return, (keys) )
FC_REFLECT( hive::plugins::clive_api::get_public_keys_return, (keys) )
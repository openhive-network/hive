#pragma once
#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/protocol/types.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>

namespace hive { namespace plugins { namespace wallet_api {

namespace detail
{
  class wallet_api_impl;
}

using plugins::json_rpc::void_type;

struct wallet_args
{
  std::string wallet_name;
};

using create_args = wallet_args;
struct create_return
{
  std::string password;
};

using open_args   = wallet_args;
using open_return = void_type;

class wallet_api
{
  public:
    wallet_api();
    ~wallet_api();

    DECLARE_API(
      (create)
      (open)
      )

  private:
    std::unique_ptr< detail::wallet_api_impl > my;
};

} } } // hive::plugins::wallet_api

FC_REFLECT( hive::plugins::wallet_api::wallet_args, (wallet_name) )
FC_REFLECT( hive::plugins::wallet_api::create_return, (password) )

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

struct create_args
{
  std::string wallet_name;
};

struct create_return
{
  std::string password;
};

class wallet_api
{
  public:
    wallet_api();
    ~wallet_api();

    DECLARE_API(
      (create)
      )

  private:
    std::unique_ptr< detail::wallet_api_impl > my;
};

} } } // hive::plugins::wallet_api

FC_REFLECT( hive::plugins::wallet_api::create_args, (wallet_name) )
FC_REFLECT( hive::plugins::wallet_api::create_return, (password) )

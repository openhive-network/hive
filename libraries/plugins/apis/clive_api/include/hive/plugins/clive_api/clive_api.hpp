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


struct set_timeout_args
{
  int64_t seconds;
};
using set_timeout_return = void_type;

using lock_all_args   = void_type;
using lock_all_return = void_type;

using lock_args   = wallet_args;
using lock_return = void_type;

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
    )

  private:
    std::unique_ptr< detail::clive_api_impl > my;
};

} } } // hive::plugins::clive_api

FC_REFLECT( hive::plugins::clive_api::wallet_args, (wallet_name) )
FC_REFLECT( hive::plugins::clive_api::create_return, (password) )
FC_REFLECT( hive::plugins::clive_api::set_timeout_args, (seconds) )

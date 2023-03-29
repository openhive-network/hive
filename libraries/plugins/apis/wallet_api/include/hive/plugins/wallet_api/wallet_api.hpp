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

typedef void_type pychol_mychol_args;

struct pychol_mychol_return
{
  variant_object any;
};

class wallet_api
{
  public:
    wallet_api();
    ~wallet_api();

    DECLARE_API(
      (pychol_mychol)
      )

  private:
    std::unique_ptr< detail::wallet_api_impl > my;
};

} } } // hive::plugins::wallet_api

FC_REFLECT( hive::plugins::wallet_api::pychol_mychol_return,
  (any)
  )

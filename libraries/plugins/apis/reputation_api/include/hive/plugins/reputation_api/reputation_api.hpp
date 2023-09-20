#pragma once
#include <hive/plugins/json_rpc/utility.hpp>
#include <hive/plugins/reputation/reputation_objects.hpp>
#include <hive/plugins/database_api/database_api_objects.hpp>

#include <hive/protocol/types.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>

namespace hive { namespace plugins { namespace reputation {

using hive::protocol::account_name_type;

namespace detail
{
  class reputation_api_impl;
}

struct account_reputation
{
  account_name_type             account;
  hive::protocol::share_type   reputation;
};

struct get_account_reputations_args
{
  account_name_type account_lower_bound;
  uint32_t          limit = 1000;
};

struct get_account_reputations_return
{
  vector< account_reputation > reputations;
};

class reputation_api
{
  public:
    reputation_api( appbase::application& app );
    ~reputation_api();

    DECLARE_API(
      (get_account_reputations)
    )

  private:
    std::unique_ptr< detail::reputation_api_impl > my;
};

} } } // hive::plugins::reputation

FC_REFLECT( hive::plugins::reputation::account_reputation,
        (account)(reputation) );

FC_REFLECT( hive::plugins::reputation::get_account_reputations_args,
        (account_lower_bound)(limit) );

FC_REFLECT( hive::plugins::reputation::get_account_reputations_return,
        (reputations) );

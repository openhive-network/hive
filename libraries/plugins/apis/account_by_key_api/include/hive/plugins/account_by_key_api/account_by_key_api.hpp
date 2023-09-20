#pragma once

#include <appbase/application.hpp>

#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/protocol/types.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>

namespace hive { namespace plugins { namespace account_by_key {

namespace detail
{
  class account_by_key_api_impl;
}

struct get_key_references_args
{
  std::vector< hive::protocol::public_key_type > keys;
};

struct get_key_references_return
{
  std::vector< std::vector< hive::protocol::account_name_type > > accounts;
};

class account_by_key_api
{
  public:
    account_by_key_api( appbase::application& app );
    ~account_by_key_api();

    DECLARE_API( (get_key_references) )

  private:
    std::unique_ptr< detail::account_by_key_api_impl > my;
};

} } } // hive::plugins::account_by_key

FC_REFLECT( hive::plugins::account_by_key::get_key_references_args,
  (keys) )

FC_REFLECT( hive::plugins::account_by_key::get_key_references_return,
  (accounts) )

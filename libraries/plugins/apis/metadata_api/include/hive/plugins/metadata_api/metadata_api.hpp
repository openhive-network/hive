#pragma once

#include <appbase/application.hpp>
#include <hive/protocol/types.hpp>
#include <hive/plugins/json_rpc/utility.hpp>

#include <vector>

namespace hive { namespace plugins { namespace metadata {

using hive::protocol::account_name_type;

struct get_account_metadata_args
{
  account_name_type account;
};

struct get_account_metadata_return
{
  std::string json_metadata;
  std::string posting_json_metadata;
};

struct find_account_metadata_args
{
  std::vector< account_name_type > accounts;
};

struct account_metadata_info
{
  account_name_type account;
  std::string       json_metadata;
  std::string       posting_json_metadata;
};

struct find_account_metadata_return
{
  std::vector< account_metadata_info > metadata;
};

namespace detail { class metadata_api_impl; }


class metadata_api
{
  public:
    metadata_api( appbase::application& app );
    ~metadata_api();

    DECLARE_API(
      (get_account_metadata)
      (find_account_metadata)
    )

  private:
    std::unique_ptr< detail::metadata_api_impl > my;
};

} } } // hive::plugins::metadata

FC_REFLECT( hive::plugins::metadata::get_account_metadata_args,
        (account) )

FC_REFLECT( hive::plugins::metadata::get_account_metadata_return,
        (json_metadata)(posting_json_metadata) )

FC_REFLECT( hive::plugins::metadata::find_account_metadata_args,
        (accounts) )

FC_REFLECT( hive::plugins::metadata::account_metadata_info,
        (account)(json_metadata)(posting_json_metadata) )

FC_REFLECT( hive::plugins::metadata::find_account_metadata_return,
        (metadata) )

#pragma once

#include <appbase/application.hpp>
#include <hive/protocol/types.hpp>
#include <hive/plugins/json_rpc/utility.hpp>


namespace hive { namespace plugins { namespace metadata {

struct get_metadata_args
{
  account_name_type account;
};

struct get_metadata_return
{
  std::string json_metadata;
  std::string posting_json_metadata;
};

namespace detail { class metadata_api_impl; }


class metadata_api
{
  public:
    metadata_api( appbase::application& app );
    ~metadata_api();

    DECLARE_API(
      (get_metadata)
    )

  private:
    std::unique_ptr< detail::metadata_api_impl > my;
};

} } } // hive::plugins::metadata

FC_REFLECT( hive::plugins::metadata::get_metadata_args,
        (account) )

FC_REFLECT( hive::plugins::metadata::get_metadata_return,
        (json_metadata)(posting_json_metadata) )

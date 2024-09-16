#pragma once

#include <appbase/application.hpp>

#include <hive/plugins/json_rpc/utility.hpp>

#include <fc/time.hpp>

namespace hive { namespace plugins { namespace app_status_api {

using hive::plugins::json_rpc::void_type;

/* get_app_status */
typedef void_type get_app_status_args;
using get_app_status_return = hive::utilities::notifications::collector_t;

namespace detail{ class app_status_api_impl; }

class app_status_api
{
  public:
    app_status_api( appbase::application& app );
    ~app_status_api();

    DECLARE_API((get_app_status))

  private:
    std::unique_ptr<detail::app_status_api_impl> my;
};

} } } // hive::plugins::app_status_api

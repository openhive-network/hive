#pragma once
#include <hive/protocol/block.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/json_rpc/utility.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>
#include <fc/time.hpp>
#include <fc/network/ip.hpp>

#include <boost/thread/mutex.hpp>

namespace hive { namespace plugins { namespace app_status_api {

using hive::plugins::json_rpc::void_type;

struct current_status
{
  std::string current_status;
};

/* get_app_status */
typedef void_type get_app_status_args;
struct get_app_status_return
{
  current_status  value;
  std::string     name;
  fc::time_point  time;
};

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

FC_REFLECT(hive::plugins::app_status_api::current_status, (current_status))
FC_REFLECT(hive::plugins::app_status_api::get_app_status_return, (value)(name)(time))

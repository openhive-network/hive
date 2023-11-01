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

namespace hive { namespace plugins { namespace node_status_api {

using std::vector;
using fc::variant;
using fc::optional;
using hive::plugins::json_rpc::void_type;

/* get_node_status */
typedef void_type get_node_status_args;
struct get_node_status_return
{
  uint32_t last_processed_block_num;
  fc::time_point_sec last_processed_block_time;
};

namespace detail{ class node_status_api_impl; }

class node_status_api
{
   public:
      node_status_api(appbase::application& app);
      ~node_status_api();

      DECLARE_API((get_node_status))

   private:
      std::unique_ptr<detail::node_status_api_impl> my;
};

} } } // hive::plugins::node_status_api

FC_REFLECT(hive::plugins::node_status_api::get_node_status_return, (last_processed_block_num)(last_processed_block_time))

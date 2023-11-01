#include <hive/plugins/node_status_api/node_status_api.hpp>
#include <hive/plugins/node_status_api/node_status_api_plugin.hpp>

#include <hive/chain/database.hpp>

#include <appbase/application.hpp>

namespace hive { namespace plugins { namespace node_status_api {

namespace detail
{
  class node_status_api_impl
  {
    public:
      node_status_api_impl( appbase::application& app ) : _db(app.get_plugin< hive::plugins::chain::chain_plugin>().db())
      {}

      DECLARE_API_IMPL((get_node_status))
      hive::chain::database& _db;
  };


  DEFINE_API_IMPL(node_status_api_impl, get_node_status)
  {
    hive::chain::database::node_status_t node_status = _db.get_node_status();

    get_node_status_return result;
    result.last_processed_block_num = node_status.last_processed_block_num;
    result.last_processed_block_time = node_status.last_processed_block_time;
    return result;
  }
} // detail

node_status_api::node_status_api( appbase::application& app ) : my(new detail::node_status_api_impl( app ))
{
  JSON_RPC_REGISTER_EARLY_API(HIVE_NODE_STATUS_API_PLUGIN_NAME);
}

node_status_api::~node_status_api() {}

DEFINE_LOCKLESS_APIS(node_status_api, (get_node_status))

} } } // hive::plugins::node_status_api

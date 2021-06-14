#include <hive/plugins/network_node_api/network_node_api.hpp>
#include <hive/plugins/network_node_api/network_node_api_plugin.hpp>

#include <appbase/application.hpp>

namespace hive { namespace plugins { namespace network_node_api {

namespace detail
{
   class network_node_api_impl
   {
      public:
         network_node_api_impl() :
            _p2p( appbase::app().get_plugin< hive::plugins::p2p::p2p_plugin >() )
            //_chain( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >() )
         {}

         DECLARE_API_IMPL(
            (get_info)
            (add_node)
            (set_allowed_peers)
            (get_connected_peers)
         )

         hive::plugins::p2p::p2p_plugin&                      _p2p;
         //hive::plugins::chain::chain_plugin&                  _chain;
   };

   DEFINE_API_IMPL( network_node_api_impl, get_info )
   {
      return _p2p.get_info();
   }

   DEFINE_API_IMPL( network_node_api_impl, add_node )
   {
      _p2p.add_node(args.endpoint);
      add_node_return result;
      return result;
   }

   DEFINE_API_IMPL( network_node_api_impl, set_allowed_peers )
   {
      _p2p.set_allowed_peers(args.allowed_peers);
      set_allowed_peers_return result;
      return result;
   }

   DEFINE_API_IMPL( network_node_api_impl, get_connected_peers )
   {
      get_connected_peers_return result;
      result.connected_peers = _p2p.get_connected_peers();
      return result;
   }

} // detail

network_node_api::network_node_api() : my( new detail::network_node_api_impl() )
{
   JSON_RPC_REGISTER_API( HIVE_NETWORK_NODE_API_PLUGIN_NAME );
}

network_node_api::~network_node_api() {}

DEFINE_LOCKLESS_APIS( network_node_api,
   (get_info)
   (add_node)
   (set_allowed_peers)
   (get_connected_peers)
)

} } } // hive::plugins::network_node_api

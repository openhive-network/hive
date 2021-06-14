#pragma once
#include <hive/protocol/block.hpp>

#include <hive/plugins/p2p/p2p_plugin.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/json_rpc/utility.hpp>

#include <graphene/net/core_messages.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>
#include <fc/network/ip.hpp>

#include <boost/thread/mutex.hpp>

namespace hive { namespace plugins { namespace network_node_api {

using std::vector;
using fc::variant;
using fc::optional;
using hive::plugins::json_rpc::void_type;
using hive::plugins::p2p::api_peer_status;

/* get_info */

typedef void_type          get_info_args;
typedef fc::variant_object get_info_return;

/* add_node */

struct add_node_args
{
   fc::ip::endpoint endpoint;
};

typedef void_type          add_node_return;

/* set_allowed_peers */

struct set_allowed_peers_args
{
   std::vector< graphene::net::node_id_t > allowed_peers;
};

typedef void_type          set_allowed_peers_return;

/* get_connected_peers */
typedef void_type     get_connected_peers_args;
struct get_connected_peers_return
{
   std::vector< api_peer_status > connected_peers;
};

namespace detail{ class network_node_api_impl; }

class network_node_api
{
   public:
      network_node_api();
      ~network_node_api();

      DECLARE_API(
         (get_info)
         (add_node)
         (set_allowed_peers)
         (get_connected_peers)
      )

   private:
      std::unique_ptr< detail::network_node_api_impl > my;
};

} } } // hive::plugins::network_node_api

FC_REFLECT( hive::plugins::network_node_api::add_node_args,
   (endpoint) )
FC_REFLECT( hive::plugins::network_node_api::set_allowed_peers_args,
   (allowed_peers) )
FC_REFLECT( hive::plugins::network_node_api::get_connected_peers_return,
   (connected_peers) )

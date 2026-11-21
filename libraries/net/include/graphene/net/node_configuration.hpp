#pragma once

#include <graphene/net/config.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/network/ip.hpp>

#include <string>
#include <vector>

namespace graphene { namespace net {

struct node_configuration
{
   node_configuration() {}

   fc::ip::endpoint listen_endpoint;
   bool accept_incoming_connections = true;
   bool wait_if_endpoint_is_busy = true;

   /**
    * Originally, our p2p code just had a 'node-id' that was a random number identifying this node
    * on the network.  This is now a private key/public key pair, where the public key is used
    * in place of the old random node-id.  The private part is unused, but might be used in
    * the future to support some notion of trusted peers.
    */
   fc::ecc::private_key private_key;

   /** if we have less than `desired_number_of_connections`, we will try to connect with more nodes */
   uint32_t desired_number_of_connections = GRAPHENE_NET_DEFAULT_DESIRED_CONNECTIONS;
   /** if we have _maximum_number_of_connections or more, we will refuse any inbound connections */
   uint32_t maximum_number_of_connections = GRAPHENE_NET_DEFAULT_MAX_CONNECTIONS;
   /**
    * connections held to one host (an IPv4 address or an IPv6 /64) at most, counting active and accepted handshaking ones; 0 = no limit.
    * A hard limit only when set in the advanced node parameters; otherwise it gives way while other hosts can't fill
    * desired_number_of_connections (a value saved in node_config.json doesn't count as set)
    */
   uint32_t maximum_connections_per_host = GRAPHENE_NET_DEFAULT_MAX_CONNECTIONS_PER_HOST;
   /**
    * addresses and subnets ("172.17.0.1", "10.0.0.0/8", "2001:db8::/32") whose connections maximum_connections_per_host
    * does not limit, e.g. a proxy that rewrites the source address of every inbound peer.  Setting it replaces the
    * default, so list loopback again to keep it exempt
    */
   std::vector<std::string> host_limit_exempt_subnets = { "127.0.0.0/8", "::1/128" };
   /** percent of `desired_number_of_connections` the connect loop keeps for each address family that has candidates (at most 50) */
   uint32_t reserved_address_family_percent = GRAPHENE_NET_DEFAULT_RESERVED_ADDRESS_FAMILY_PERCENT;
   /** retry connections to peers that have failed or rejected us this often, in seconds */
   uint32_t peer_connection_retry_timeout = GRAPHENE_NET_DEFAULT_PEER_CONNECTION_RETRY_TIME;
   /** how many seconds of inactivity are permitted before disconnecting a peer */
   uint32_t peer_inactivity_timeout = GRAPHENE_NET_PEER_HANDSHAKE_INACTIVITY_TIMEOUT;
   /**
    * seconds an active peer may give us no useful data (blocks that link, timely advertisements of blocks we have)
    * while our head is behind and another peer delivered blocks, before it is disconnected and replaced; it may not
    * take a slot again for as long.  0 = never, the default.  Keep it above GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC so sync is restarted with the peers we have first
    */
   uint32_t idle_peer_disconnect_timeout = GRAPHENE_NET_DEFAULT_IDLE_PEER_DISCONNECT_TIMEOUT_SEC;

   bool peer_advertising_disabled = false;

   uint32_t maximum_number_of_blocks_to_handle_at_one_time = GRAPHENE_NET_MAX_NUMBER_OF_BLOCKS_TO_HANDLE_AT_ONE_TIME;
   uint32_t maximum_number_of_sync_blocks_to_prefetch = GRAPHENE_NET_MAX_NUMBER_OF_BLOCKS_TO_PREFETCH;
   uint32_t maximum_blocks_per_peer_during_syncing = GRAPHENE_NET_MAX_BLOCKS_PER_PEER_DURING_SYNCING;
   int64_t active_ignored_request_timeout_microseconds = 6000000;
   /** how long, at most, transaction traffic queued for a peer waits for more to share its socket write; 0 = no wait */
   int64_t transaction_flush_delay_microseconds = 1000;
   /** how long, at least, between two runs of the fetch items loop that only transactions trigger; 0 = no minimum */
   int64_t fetch_items_minimum_interval_microseconds = 5000;
   /** how many transactions received from peers may wait for validation at once; at the limit, a peer's next one waits. 0 = no limit */
   uint32_t maximum_transactions_in_flight = 10000;
};

} }

FC_REFLECT(graphene::net::node_configuration,
   (listen_endpoint)
   (accept_incoming_connections)
   (wait_if_endpoint_is_busy)
   (private_key)
   (desired_number_of_connections)
   (maximum_number_of_connections)
   (maximum_connections_per_host)
   (host_limit_exempt_subnets)
   (reserved_address_family_percent)
   (peer_connection_retry_timeout)
   (peer_inactivity_timeout)
   (idle_peer_disconnect_timeout)
   (peer_advertising_disabled)
   (maximum_number_of_blocks_to_handle_at_one_time)
   (maximum_number_of_sync_blocks_to_prefetch)
   (maximum_blocks_per_peer_during_syncing)
   (active_ignored_request_timeout_microseconds)
   (transaction_flush_delay_microseconds)
   (fetch_items_minimum_interval_microseconds)
   (maximum_transactions_in_flight)
)

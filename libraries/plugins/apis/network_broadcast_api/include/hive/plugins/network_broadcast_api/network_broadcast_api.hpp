#pragma once
#include <hive/protocol/block.hpp>

#include <hive/plugins/p2p/p2p_plugin.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/json_rpc/utility.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>

#include <boost/thread/mutex.hpp>

namespace hive { namespace plugins { namespace network_broadcast_api {

using std::vector;
using fc::variant;
using fc::optional;
using hive::plugins::json_rpc::void_type;

using hive::protocol::signed_transaction;
using hive::protocol::transaction_id_type;
using hive::protocol::signed_block;

struct broadcast_transaction_args
{
  signed_transaction   trx;
  int32_t              max_block_age = -1;
};

typedef void_type broadcast_transaction_return;

struct broadcast_block_args
{
  signed_block   block;
};

typedef void_type broadcast_block_return;

/**
 * The block that included the transaction, or with `expired` set, the block that passed its expiration without it.
 * Result of `broadcast_transaction_synchronous` of condenser_api and wallet_bridge_api.
 */
struct broadcast_transaction_synchronous_return
{
  transaction_id_type   id;
  int32_t               block_num = 0;
  int32_t               trx_num = 0;
  optional< int64_t >   rc_cost;
  bool                  expired = false;
};

namespace detail{ class network_broadcast_api_impl; }

class network_broadcast_api
{
  public:
    network_broadcast_api( appbase::application& app );
    ~network_broadcast_api();

    DECLARE_API(
      (broadcast_transaction)
    )

    void check_p2p();

    /**
     * Accepts and broadcasts the transaction, then waits until a block includes it or passes its expiration.
     * Implements `broadcast_transaction_synchronous` of condenser_api and wallet_bridge_api; not a JSON-RPC method.
     * Throws if the node stops applying blocks first (e.g. it is shutting down): the transaction was accepted then,
     * but whether a block will include it is unknown.
     */
    broadcast_transaction_synchronous_return broadcast_transaction_synchronous( const signed_transaction& trx );

  private:
    std::unique_ptr< detail::network_broadcast_api_impl > my;
};

} } } // hive::plugins::network_broadcast_api

FC_REFLECT( hive::plugins::network_broadcast_api::broadcast_transaction_args,
  (trx)(max_block_age) )

FC_REFLECT( hive::plugins::network_broadcast_api::broadcast_block_args,
  (block) )

FC_REFLECT( hive::plugins::network_broadcast_api::broadcast_transaction_synchronous_return,
  (id)(block_num)(trx_num)(rc_cost)(expired) )

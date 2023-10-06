#include <hive/plugins/network_broadcast_api/network_broadcast_api.hpp>
#include <hive/plugins/network_broadcast_api/network_broadcast_api_plugin.hpp>

#include <appbase/application.hpp>

namespace hive { namespace plugins { namespace network_broadcast_api {

namespace detail
{
  class network_broadcast_api_impl
  {
    public:
      network_broadcast_api_impl( appbase::application& app ) :
        _p2p( app.get_plugin< hive::plugins::p2p::p2p_plugin >() ),
        _chain( app.get_plugin< hive::plugins::chain::chain_plugin >() )
      {}

      DECLARE_API_IMPL(
        (broadcast_transaction)
      )

      bool check_max_block_age( int32_t max_block_age ) const;

      hive::plugins::p2p::p2p_plugin&                      _p2p;
      hive::plugins::chain::chain_plugin&                  _chain;
  };

  DEFINE_API_IMPL( network_broadcast_api_impl, broadcast_transaction )
  {
    FC_ASSERT( !check_max_block_age( args.max_block_age ) );

    hive::chain::full_transaction_ptr full_transaction;
    _chain.determine_encoding_and_accept_transaction( full_transaction, args.trx );
    _p2p.broadcast_transaction(full_transaction);

    return broadcast_transaction_return();
  }

  bool network_broadcast_api_impl::check_max_block_age( int32_t max_block_age ) const
  {
    if( max_block_age < 0 )
      return false;

    return _chain.db().with_read_lock( [&]()
    {
      fc::time_point_sec now = fc::time_point::now();
      const auto& dgpo = _chain.db().get_dynamic_global_properties();

      return ( dgpo.time < now - fc::seconds( max_block_age ) );
    }, fc::seconds(1));
  }

} // detail

network_broadcast_api::network_broadcast_api( appbase::application& app ) : my( new detail::network_broadcast_api_impl( app ) )
{
  JSON_RPC_REGISTER_API( HIVE_NETWORK_BROADCAST_API_PLUGIN_NAME, app );
}

network_broadcast_api::~network_broadcast_api() {}

DEFINE_LOCKLESS_APIS( network_broadcast_api,
  (broadcast_transaction)
)

} } } // hive::plugins::network_broadcast_api

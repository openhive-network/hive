
#include <hive/plugins/network_broadcast_api/network_broadcast_api.hpp>
#include <hive/plugins/network_broadcast_api/network_broadcast_api_plugin.hpp>

#include <appbase/application.hpp>

namespace hive { namespace plugins { namespace network_broadcast_api {

namespace detail
{
  class network_broadcast_api_impl
  {
    public:
      network_broadcast_api_impl() :
        _p2p( appbase::app().get_plugin< hive::plugins::p2p::p2p_plugin >() ),
        _chain( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >() )
      {}

      DECLARE_API_IMPL(
        (broadcast_transaction)
        (broadcast_block)
      )

      bool check_max_block_age( int32_t max_block_age ) const;

      hive::plugins::p2p::p2p_plugin&                      _p2p;
      hive::plugins::chain::chain_plugin&                  _chain;
  };

  DEFINE_API_IMPL( network_broadcast_api_impl, broadcast_transaction )
  {
    FC_ASSERT( !check_max_block_age( args.max_block_age ) );

    std::shared_ptr<hive::chain::full_transaction_type> full_transaction = _chain.determine_encoding_and_accept_transaction(args.trx);
    _p2p.broadcast_transaction(full_transaction);

    return broadcast_transaction_return();
  }

  DEFINE_API_IMPL( network_broadcast_api_impl, broadcast_block )
  {
    // TODO: analyze this API -- will it be a problem accepting JSON transactions where the serialization
    // is not known?  methinks it will
    std::shared_ptr<hive::chain::full_block_type> full_block = hive::chain::full_block_type::create_from_signed_block(args.block);

    _chain.accept_block(full_block, /*currently syncing*/ false, /*skip*/ chain::database::skip_nothing);
    _p2p.broadcast_block(full_block);
    return broadcast_block_return();
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

network_broadcast_api::network_broadcast_api() : my( new detail::network_broadcast_api_impl() )
{
  JSON_RPC_REGISTER_API( HIVE_NETWORK_BROADCAST_API_PLUGIN_NAME );
}

network_broadcast_api::~network_broadcast_api() {}

DEFINE_LOCKLESS_APIS( network_broadcast_api,
  (broadcast_transaction)
  (broadcast_block)
)

} } } // hive::plugins::network_broadcast_api

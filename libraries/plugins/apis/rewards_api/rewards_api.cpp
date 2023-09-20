#include <chainbase/chainbase.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/uint256.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/rewards_api/rewards_api_plugin.hpp>
#include <hive/plugins/rewards_api/rewards_api.hpp>

namespace hive { namespace plugins { namespace rewards_api {

namespace detail {

class rewards_api_impl
{
public:
  rewards_api_impl( appbase::application& app ) :
    _db( app.get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}

  DECLARE_API_IMPL( (simulate_curve_payouts) );

  chain::database& _db;
};

DEFINE_API_IMPL( rewards_api_impl, simulate_curve_payouts )
{
  FC_ASSERT( false, "Not supported" );
}

} // hive::plugins::rewards_api::detail

rewards_api::rewards_api( appbase::application& app ) : my( std::make_unique< detail::rewards_api_impl >( app ) )
{
  JSON_RPC_REGISTER_API( HIVE_REWARDS_API_PLUGIN_NAME, app );
}

rewards_api::~rewards_api() {}

DEFINE_READ_APIS( rewards_api, (simulate_curve_payouts) )

} } } // hive::plugins::rewards_api


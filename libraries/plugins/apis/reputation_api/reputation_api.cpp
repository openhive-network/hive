#include <hive/plugins/reputation_api/reputation_api_plugin.hpp>
#include <hive/plugins/reputation_api/reputation_api.hpp>

#include <hive/plugins/reputation/reputation_objects.hpp>

namespace hive { namespace plugins { namespace reputation {

namespace detail {

class reputation_api_impl
{
  public:
    reputation_api_impl( appbase::application& app ) : _db( app.get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}

    DECLARE_API_IMPL(
      (get_account_reputations)
    )

    chain::database& _db;
};

DEFINE_API_IMPL( reputation_api_impl, get_account_reputations )
{
  FC_ASSERT( args.limit <= 1000, "Cannot retrieve more than 1000 account reputations at a time." );

  const auto& acc_idx = _db.get_index< chain::account_index >().indices().get< chain::by_name >();
  const auto& rep_idx = _db.get_index< reputation::reputation_index >().indices().get< reputation::by_account >();

  auto acc_itr = acc_idx.lower_bound( args.account_lower_bound );

  get_account_reputations_return result;
  result.reputations.reserve( args.limit );

  while( acc_itr != acc_idx.end() && result.reputations.size() < args.limit )
  {
    auto itr = rep_idx.find( acc_itr->get_name() );
    account_reputation rep;

    rep.account = acc_itr->get_name();
    rep.reputation = itr != rep_idx.end() ? itr->reputation : 0;

    result.reputations.push_back( rep );
    ++acc_itr;
  }

  return result;
}

} // detail

reputation_api::reputation_api( appbase::application& app ): my( new detail::reputation_api_impl( app ) )
{
  JSON_RPC_REGISTER_API( HIVE_REPUTATION_API_PLUGIN_NAME, app );
}

reputation_api::~reputation_api() {}

DEFINE_READ_APIS( reputation_api,
  (get_account_reputations)
)

} } } // hive::plugins::reputation

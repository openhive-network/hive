#include <hive/plugins/clive_api/clive_api_plugin.hpp>
#include <hive/plugins/clive_api/clive_api.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/chain/database.hpp>

namespace hive { namespace plugins { namespace clive_api {

class clive_api::impl final
{
  public:
    impl() : _chain( appbase::app().get_plugin<hive::plugins::chain::chain_plugin>() ), _db(_chain.db()) {}

    DECLARE_API_IMPL(
      (collect_dashboard_data)
    )

  private:
    hive::plugins::chain::chain_plugin& _chain;
  public:
    /// public because of DEFINE_READ_APIS needs
    hive::plugins::chain::database& _db;
};

DEFINE_API_IMPL(clive_api::impl, collect_dashboard_data)
{
  collect_dashboard_data_return retval;

  return retval;
}

void clive_api::deleter::operator()(impl* p) const
{
  delete p;
}

clive_api::clive_api() : my( new impl() )
{
  JSON_RPC_REGISTER_API( HIVE_CLIVE_API_PLUGIN_NAME );
}

DEFINE_READ_APIS( clive_api,
  (collect_dashboard_data)
)

} } } //hive::plugins::chain

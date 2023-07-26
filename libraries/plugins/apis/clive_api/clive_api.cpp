#include <hive/plugins/clive_api/clive_api_plugin.hpp>
#include <hive/plugins/clive_api/clive_api.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/chain/database.hpp>

#include <hive/plugins/database_api/database_api_objects.hpp>
#include <hive/plugins/database_api/database_api.hpp>

#define CLIVE_API_SINGLE_QUERY_LIMIT 100

namespace hive { namespace plugins { namespace clive_api {

class clive_api::impl final
{
  public:
    explicit impl(appbase::application& the_app);

    DECLARE_API_IMPL(
      (collect_dashboard_data)
    )

  private:
    void fill(const hive::chain::util::manabar& source, api_manabar* data);
    void collect_dashboard_data(const hive::chain::account_object& account, account_dashboard_data* data);

    hive::plugins::chain::chain_plugin& _chain;
    std::shared_ptr<hive::plugins::database_api::database_api> _database_api;
    std::shared_ptr<hive::plugins::account_history::account_history_api> _account_history_api;
    std::shared_ptr<hive::plugins::rc::rc_api> _rc_api;
    std::shared_ptr<hive::plugins::reputation::reputation_api> _repuatation_api;

  public:
    /// public because of DEFINE_READ_APIS needs
    hive::plugins::chain::database& _db;
};

clive_api::impl::impl(appbase::application& the_app) : _chain(the_app.get_plugin<hive::plugins::chain::chain_plugin>()), _db(_chain.db())
{
  {
    auto p = the_app.find_plugin<hive::plugins::database_api::database_api_plugin>();
    if (p != nullptr)
      _database_api = p->api;
  }

  {
    auto p = the_app.find_plugin<hive::plugins::account_history::account_history_api_plugin>();
    if (p != nullptr)
      _account_history_api = p->api;
  }

  {
    auto p = the_app.find_plugin<hive::plugins::rc::rc_api_plugin>();
    if (p != nullptr)
      _rc_api = p->api;
  }

  {
    auto p = the_app.find_plugin<hive::plugins::reputation::reputation_api_plugin>();
    if (p != nullptr)
      _repuatation_api = p->api;
  }

  FC_ASSERT(_database_api, "Required database_api is missing");
  FC_ASSERT(_account_history_api, "Required account_history_api is missing");
  FC_ASSERT(_rc_api, "Required rc_api is missing");
  FC_ASSERT(_repuatation_api, "Required reputation_api is missing");
}

void clive_api::impl::fill(const hive::chain::util::manabar& source, api_manabar* data)
{
  data->current_mana = source.current_mana;
  data->last_update_time = source.last_update_time;
}

void clive_api::impl::collect_dashboard_data(const hive::chain::account_object& account, account_dashboard_data* data)
{
  fill(account.voting_manabar, &data->voting_manabar);
  fill(account.downvote_manabar, &data->downvote_manabar);
  fill(account.rc_manabar, &data->rc_manabar);


}

DEFINE_API_IMPL(clive_api::impl, collect_dashboard_data)
{
  FC_ASSERT(args.accounts.size() <= CLIVE_API_SINGLE_QUERY_LIMIT,
    "list of accounts passed to collect_dashboard_data too big");

  collect_dashboard_data_return retval;

  auto dgpo = _database_api->get_dynamic_global_properties({});
  retval.head_block_number = dgpo.head_block_number;
  retval.head_block_time = dgpo.time;
  retval.total_vesting_fund_hive = dgpo.total_vesting_fund_hive;
  retval.total_vesting_shares = dgpo.total_vesting_fund_hive;

  for (auto& a : args.accounts)
  {
    auto acct = _db.find<chain::account_object, chain::by_name >(a);
    if (acct != nullptr)
    {
      auto ei = retval.collected_account_infos.emplace(a, account_dashboard_data());

      collect_dashboard_data(*acct , &ei.first->second);
    }
  }

  return retval;
}

void clive_api::deleter::operator()(impl* p) const
{
  delete p;
}

clive_api::clive_api() : my( new impl(appbase::app()) )
{
  JSON_RPC_REGISTER_API( HIVE_CLIVE_API_PLUGIN_NAME );
}

DEFINE_READ_APIS( clive_api,
  (collect_dashboard_data)
)

} } } //hive::plugins::chain

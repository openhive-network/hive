#include <hive/plugins/clive_api/clive_api_plugin.hpp>
#include <hive/plugins/clive_api/clive_api.hpp>

#include <fc/variant_object.hpp>
#include <fc/reflect/variant.hpp>

namespace hive { namespace plugins { namespace clive_api {

namespace detail {

using hive::plugins::clive::wallet_manager;

class clive_api_impl
{
  public:
    clive_api_impl(): _wallet_mgr( appbase::app().get_plugin<hive::plugins::clive::clive_plugin>().get_wallet_manager() ) {}

    DECLARE_API_IMPL
    (
      (create)
      (open)
      (set_timeout)
      (lock_all)
      (lock)
      (unlock)
    )

    wallet_manager& _wallet_mgr;
};

DEFINE_API_IMPL( clive_api_impl, create )
{
  return { _wallet_mgr.create( args.wallet_name ) };
}

DEFINE_API_IMPL( clive_api_impl, open )
{
  _wallet_mgr.open( args.wallet_name );
  return open_return();
}

DEFINE_API_IMPL( clive_api_impl, set_timeout )
{
  _wallet_mgr.set_timeout( args.seconds );
  return set_timeout_return();
}

DEFINE_API_IMPL( clive_api_impl, lock_all )
{
  _wallet_mgr.lock_all();
  return lock_all_return();
}

DEFINE_API_IMPL( clive_api_impl, lock )
{
  _wallet_mgr.lock( args.wallet_name );
  return lock_return();
}

DEFINE_API_IMPL( clive_api_impl, unlock )
{
  _wallet_mgr.unlock( args.wallet_name, args.password );
  return unlock_return();
}

} // detail

clive_api::clive_api(): my( new detail::clive_api_impl() )
{
  JSON_RPC_REGISTER_API( HIVE_CLIVE_API_PLUGIN_NAME );
}

clive_api::~clive_api() {}

DEFINE_LOCKLESS_APIS( clive_api,
  (create)
  (open)
  (set_timeout)
  (lock_all)
  (lock)
  (unlock)
  )

} } } // hive::plugins::clive_api

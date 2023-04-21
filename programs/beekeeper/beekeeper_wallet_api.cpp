#include <beekeeper/beekeeper_wallet_api.hpp>
#include <beekeeper/beekeeper_wallet_base.hpp>

#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <fc/variant_object.hpp>
#include <fc/reflect/variant.hpp>

namespace beekeeper {

#define HIVE_BEEKEEPER_API_NAME "beekeeper_api"

namespace detail {

using beekeeper::beekeeper_wallet_manager;

class beekeeper_api_impl
{
  public:
    beekeeper_api_impl( beekeeper::beekeeper_wallet_manager& wallet_mgr ): _wallet_mgr( wallet_mgr ) {}

    DECLARE_API_IMPL
    (
      (create)
      (open)
      (set_timeout)
      (lock_all)
      (lock)
      (unlock)
      (import_key)
      (remove_key)
      (create_key)
      (list_wallets)
      (list_keys)
      (get_public_keys)
      (sign_digest)
      (get_info)
    )

    beekeeper::beekeeper_wallet_manager& _wallet_mgr;
};

DEFINE_API_IMPL( beekeeper_api_impl, create )
{
  return { _wallet_mgr.create( args.wallet_name ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, open )
{
  _wallet_mgr.open( args.wallet_name );
  return open_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, set_timeout )
{
  _wallet_mgr.set_timeout( args.seconds );
  return set_timeout_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, lock_all )
{
  _wallet_mgr.lock_all();
  return lock_all_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, lock )
{
  _wallet_mgr.lock( args.wallet_name );
  return lock_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, unlock )
{
  _wallet_mgr.unlock( args.wallet_name, args.password );
  return unlock_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, import_key )
{
  return { _wallet_mgr.import_key( args.wallet_name, args.wif_key ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, remove_key )
{
  _wallet_mgr.remove_key( args.wallet_name, args.password, args.public_key );
  return remove_key_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, create_key )
{
  return { _wallet_mgr.create_key( args.wallet_name ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, list_wallets )
{
  return { _wallet_mgr.list_wallets() };
}

DEFINE_API_IMPL( beekeeper_api_impl, list_keys )
{
  return { _wallet_mgr.list_keys( args.wallet_name, args.password) };
}

DEFINE_API_IMPL( beekeeper_api_impl, get_public_keys )
{
  return { _wallet_mgr.get_public_keys() };
}

DEFINE_API_IMPL( beekeeper_api_impl, sign_digest )
{
  using namespace beekeeper;
  return { _wallet_mgr.sign_digest( digest_type( args.digest ), public_key_type::from_base58_with_prefix( args.public_key, HIVE_ADDRESS_PREFIX ) ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, get_info )
{
  return _wallet_mgr.get_info();
}

} // detail

beekeeper_wallet_api::beekeeper_wallet_api( beekeeper::beekeeper_wallet_manager& wallet_mgr ): my( new detail::beekeeper_api_impl( wallet_mgr ) )
{
  JSON_RPC_REGISTER_API( HIVE_BEEKEEPER_API_NAME );
}

beekeeper_wallet_api::~beekeeper_wallet_api() {}

DEFINE_LOCKLESS_APIS( beekeeper_wallet_api,
  (create)
  (open)
  (set_timeout)
  (lock_all)
  (lock)
  (unlock)
  (import_key)
  (remove_key)
  (create_key)
  (list_wallets)
  (list_keys)
  (get_public_keys)
  (sign_digest)
  (get_info)
  )

} // beekeeper

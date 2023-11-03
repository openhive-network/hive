#include <beekeeper/beekeeper_wallet_api.hpp>
#include <beekeeper/extended_api.hpp>
#include <core/beekeeper_wallet_base.hpp>

#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <fc/variant_object.hpp>
#include <fc/reflect/variant.hpp>

namespace beekeeper {

#define HIVE_BEEKEEPER_API_NAME "beekeeper_api"

namespace detail {

using beekeeper::beekeeper_wallet_manager;

class beekeeper_api_impl
{
  private:
  
    extended_api ex_api;

    std::mutex mtx;

  public:
    beekeeper_api_impl( std::shared_ptr<beekeeper::beekeeper_wallet_manager> wallet_mgr ): _wallet_mgr( wallet_mgr ) {}

    DECLARE_API_IMPL
    (
      (create)
      (open)
      (close)
      (set_timeout)
      (lock_all)
      (lock)
      (unlock)
      (import_key)
      (remove_key)
      (list_wallets)
      (list_created_wallets)
      (get_public_keys)
      (sign_digest)
      (get_info)
      (create_session)
      (close_session)
      (has_matching_private_key)
    )

    std::shared_ptr<beekeeper::beekeeper_wallet_manager> _wallet_mgr;
};

DEFINE_API_IMPL( beekeeper_api_impl, create )
{
  std::lock_guard<std::mutex> guard( mtx );

  return { _wallet_mgr->create( args.token, args.wallet_name, args.password ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, open )
{
  std::lock_guard<std::mutex> guard( mtx );

  _wallet_mgr->open( args.token, args.wallet_name );
  return open_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, close )
{
  std::lock_guard<std::mutex> guard( mtx );

  _wallet_mgr->close( args.token, args.wallet_name );
  return close_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, set_timeout )
{
  std::lock_guard<std::mutex> guard( mtx );

  _wallet_mgr->set_timeout( args.token, args.seconds );
  return set_timeout_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, lock_all )
{
  std::lock_guard<std::mutex> guard( mtx );

  _wallet_mgr->lock_all( args.token );
  return lock_all_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, lock )
{
  std::lock_guard<std::mutex> guard( mtx );

  _wallet_mgr->lock( args.token, args.wallet_name );
  return lock_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, unlock )
{
  std::lock_guard<std::mutex> guard( mtx );

  if( ex_api.enabled() )
    _wallet_mgr->unlock( args.token, args.wallet_name, args.password );
  else
    FC_ASSERT(false, "unlock is not accessible");

  return unlock_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, import_key )
{
  std::lock_guard<std::mutex> guard( mtx );

  return { _wallet_mgr->import_key( args.token, args.wallet_name, args.wif_key ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, remove_key )
{
  std::lock_guard<std::mutex> guard( mtx );

  _wallet_mgr->remove_key( args.token, args.wallet_name, args.password, args.public_key );
  return remove_key_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, list_wallets )
{
  std::lock_guard<std::mutex> guard( mtx );

  return { _wallet_mgr->list_wallets( args.token ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, list_created_wallets )
{
  std::lock_guard<std::mutex> guard( mtx );

  return { _wallet_mgr->list_created_wallets( args.token ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, get_public_keys )
{
  std::lock_guard<std::mutex> guard( mtx );

  auto _keys = _wallet_mgr->get_public_keys( args.token, args.wallet_name );
  return { utility::get_public_keys( _keys ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, sign_digest )
{
  std::lock_guard<std::mutex> guard( mtx );

  using namespace beekeeper;
  return { _wallet_mgr->sign_digest( args.token, digest_type( args.sig_digest ), args.public_key, args.wallet_name ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, get_info )
{
  std::lock_guard<std::mutex> guard( mtx );

  return _wallet_mgr->get_info( args.token );
} 

DEFINE_API_IMPL( beekeeper_api_impl, create_session )
{
  std::lock_guard<std::mutex> guard( mtx );

  return { _wallet_mgr->create_session( args.salt, args.notifications_endpoint ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, close_session )
{
  std::lock_guard<std::mutex> guard( mtx );

  _wallet_mgr->close_session( args.token );
  return close_session_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, has_matching_private_key )
{
  std::lock_guard<std::mutex> guard( mtx );

  return { _wallet_mgr->has_matching_private_key( args.token, args.wallet_name, args.public_key ) };
}

} // detail

beekeeper_wallet_api::beekeeper_wallet_api( std::shared_ptr<beekeeper::beekeeper_wallet_manager> wallet_mgr, appbase::application& app ): my( new detail::beekeeper_api_impl( wallet_mgr ) )
{
  JSON_RPC_REGISTER_API( HIVE_BEEKEEPER_API_NAME );
}

beekeeper_wallet_api::~beekeeper_wallet_api() {}

DEFINE_LOCKLESS_APIS( beekeeper_wallet_api,
  (create)
  (open)
  (close)
  (set_timeout)
  (lock_all)
  (lock)
  (unlock)
  (import_key)
  (remove_key)
  (list_wallets)
  (list_created_wallets)
  (get_public_keys)
  (sign_digest)
  (get_info)
  (create_session)
  (close_session)
  (has_matching_private_key)
  )

} // beekeeper

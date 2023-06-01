#include <extension/beekeeper_wallet_api.hpp>
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
      (get_public_keys)
      (sign_digest)
      (sign_transaction)
      (get_info)
      (create_session)
      (close_session)
    )

    std::shared_ptr<beekeeper::beekeeper_wallet_manager> _wallet_mgr;
};

DEFINE_API_IMPL( beekeeper_api_impl, create )
{
  return { _wallet_mgr->create( args.token, args.wallet_name, args.password ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, open )
{
  _wallet_mgr->open( args.token, args.wallet_name );
  return open_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, close )
{
  _wallet_mgr->close( args.token, args.wallet_name );
  return close_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, set_timeout )
{
  _wallet_mgr->set_timeout( args.token, args.seconds );
  return set_timeout_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, lock_all )
{
  _wallet_mgr->lock_all( args.token );
  return lock_all_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, lock )
{
  _wallet_mgr->lock( args.token, args.wallet_name );
  return lock_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, unlock )
{
  _wallet_mgr->unlock( args.token, args.wallet_name, args.password );
  return unlock_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, import_key )
{
  return { _wallet_mgr->import_key( args.token, args.wallet_name, args.wif_key ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, remove_key )
{
  _wallet_mgr->remove_key( args.token, args.wallet_name, args.password, args.public_key );
  return remove_key_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, list_wallets )
{
  return { _wallet_mgr->list_wallets( args.token ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, get_public_keys )
{
  auto _keys = _wallet_mgr->get_public_keys( args.token );
  flat_set<std::string> _result;

  std::transform( _keys.begin(), _keys.end(), std::inserter( _result, _result.end() ),
  []( const public_key_type& public_key ){ return public_key_type::to_base58( public_key, false/*is_sha256*/ ); } );

  return { _result };
}

DEFINE_API_IMPL( beekeeper_api_impl, sign_digest )
{
  using namespace beekeeper;
  return { _wallet_mgr->sign_digest( args.token, public_key_type::from_base58( args.public_key, false/*is_sha256*/ ), digest_type( args.sig_digest ) ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, sign_transaction )
{
  using namespace beekeeper;
  return { _wallet_mgr->sign_transaction( args.token, args.transaction, args.chain_id, public_key_type::from_base58( args.public_key, false/*is_sha256*/ ), digest_type( args.sig_digest ) ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, get_info )
{
  return _wallet_mgr->get_info( args.token );
}

DEFINE_API_IMPL( beekeeper_api_impl, create_session )
{
  return { _wallet_mgr->create_session( args.salt, args.notifications_endpoint ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, close_session )
{
  _wallet_mgr->close_session( args.token );
  return close_session_return();
}

} // detail

beekeeper_wallet_api::beekeeper_wallet_api( std::shared_ptr<beekeeper::beekeeper_wallet_manager> wallet_mgr ): my( new detail::beekeeper_api_impl( wallet_mgr ) )
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
  (get_public_keys)
  (sign_digest)
  (sign_transaction)
  (get_info)
  (create_session)
  (close_session)
  )

} // beekeeper

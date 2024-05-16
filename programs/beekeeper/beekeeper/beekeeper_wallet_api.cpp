#include <beekeeper/beekeeper_wallet_api.hpp>
#include <beekeeper/extended_api.hpp>
#include <core/beekeeper_wallet_base.hpp>

#include <hive/protocol/config.hpp>

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

    const std::string prefix;
  
    extended_api ex_api;

    std::mutex mtx;

    public_key_type create( const std::string& source )
    {
      return utility::public_key::create( source, prefix );
    }

  public:
    beekeeper_api_impl( std::shared_ptr<beekeeper::beekeeper_wallet_manager> wallet_mgr, uint64_t unlock_interval )
                      : prefix( HIVE_ADDRESS_PREFIX/*At now this is only one allowed prefix, by maybe in the future custom prefixes could be used as well.*/ ),
                        ex_api( unlock_interval ), _wallet_mgr( wallet_mgr ) {}

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
      (encrypt_data)
      (decrypt_data)
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

  if( ex_api.unlock_allowed() )
  {
    try
    {
      _wallet_mgr->unlock( args.token, args.wallet_name, args.password );
    }
    FC_CAPTURE_CALL_LOG_AND_RETHROW(([this]()
      {
        ex_api.was_error();
      }), ());
  }
  else
    FC_ASSERT(false, "unlock is not accessible");

  return unlock_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, import_key )
{
  std::lock_guard<std::mutex> guard( mtx );

  return { _wallet_mgr->import_key( args.token, args.wallet_name, args.wif_key, prefix ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, remove_key )
{
  std::lock_guard<std::mutex> guard( mtx );

  _wallet_mgr->remove_key( args.token, args.wallet_name, create( args.public_key ) );
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
  return { _wallet_mgr->sign_digest( args.token, args.wallet_name, args.sig_digest, create( args.public_key ), prefix ) };
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

  return { _wallet_mgr->has_matching_private_key( args.token, args.wallet_name, create( args.public_key ) ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, encrypt_data )
{
  std::lock_guard<std::mutex> guard( mtx );

  return { _wallet_mgr->encrypt_data( args.token, create( args.from_public_key ), create( args.to_public_key ), args.wallet_name, args.content, args.nonce, prefix ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, decrypt_data )
{
  std::lock_guard<std::mutex> guard( mtx );

  return { _wallet_mgr->decrypt_data( args.token, create( args.from_public_key ), create( args.to_public_key ), args.wallet_name, args.encrypted_content ) };
}

} // detail

beekeeper_wallet_api::beekeeper_wallet_api( std::shared_ptr<beekeeper::beekeeper_wallet_manager> wallet_mgr, appbase::application& app, uint64_t unlock_interval )
                    : my( new detail::beekeeper_api_impl( wallet_mgr, unlock_interval ) )
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
  (encrypt_data)
  (decrypt_data)
  )

} // beekeeper

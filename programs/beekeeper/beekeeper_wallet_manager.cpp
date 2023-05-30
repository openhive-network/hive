#include <beekeeper/beekeeper_wallet_manager.hpp>

#include <appbase/application.hpp>

namespace beekeeper {

beekeeper_wallet_manager::beekeeper_wallet_manager( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit,
                                                    close_all_sessions_action_method&& method
                                                  )
                          : unlock_timeout( cmd_unlock_timeout ), session_limit( cmd_session_limit ), close_all_sessions_action( method )
{
  singleton = std::make_unique<singleton_beekeeper>( cmd_wallet_dir );
}

bool beekeeper_wallet_manager::start()
{
  return singleton->start();
}

void beekeeper_wallet_manager::set_timeout( const std::string& token, uint64_t secs )
{
  sessions.set_timeout( token, std::chrono::seconds( secs ) );
}

std::string beekeeper_wallet_manager::create( const std::string& token, const std::string& name, fc::optional<std::string> password )
{
  sessions.check_timeout( token );

  return sessions.get_wallet_manager( token )->create( [this]( const std::string& name ){ return singleton->create_wallet_filename( name ); }, name, password );
}

void beekeeper_wallet_manager::open( const std::string& token, const std::string& name )
{
  sessions.check_timeout( token );

  sessions.get_wallet_manager( token )->open( [this]( const std::string& name ){ return singleton->create_wallet_filename( name ); }, name );
}

void beekeeper_wallet_manager::close( const std::string& token, const std::string& name )
{
  sessions.check_timeout( token );

  sessions.get_wallet_manager( token )->close( name );
}

std::vector<wallet_details> beekeeper_wallet_manager::list_wallets( const std::string& token )
{
  sessions.check_timeout( token );
  return sessions.get_wallet_manager( token )->list_wallets();
}

map<public_key_type, private_key_type> beekeeper_wallet_manager::list_keys( const std::string& token, const string& name, const string& pw )
{
  sessions.check_timeout( token );
  return sessions.get_wallet_manager( token )->list_keys( name, pw );
}

flat_set<public_key_type> beekeeper_wallet_manager::get_public_keys( const std::string& token )
{
  sessions.check_timeout( token );
  return sessions.get_wallet_manager( token )->get_public_keys();
}

void beekeeper_wallet_manager::lock_all( const std::string& token )
{
  sessions.get_wallet_manager( token )->lock_all();
}

void beekeeper_wallet_manager::lock( const std::string& token, const std::string& name )
{
  sessions.check_timeout( token );
  sessions.get_wallet_manager( token )->lock( name );
}

void beekeeper_wallet_manager::unlock( const std::string& token, const std::string& name, const std::string& password )
{
  sessions.check_timeout( token );
  sessions.get_wallet_manager( token )->unlock( [this]( const std::string& name ){ return singleton->create_wallet_filename( name ); }, name, password );
}

string beekeeper_wallet_manager::import_key( const std::string& token, const std::string& name, const std::string& wif_key )
{
  sessions.check_timeout( token );
  return sessions.get_wallet_manager( token )->import_key( name, wif_key );
}

void beekeeper_wallet_manager::remove_key( const std::string& token, const std::string& name, const std::string& password, const std::string& public_key )
{
  sessions.check_timeout( token );
  sessions.get_wallet_manager( token )->remove_key( name, password, public_key );
}

signature_type beekeeper_wallet_manager::sign_digest( const std::string& token, const public_key_type& public_key, const digest_type& sig_digest )
{
  sessions.check_timeout( token );
  return sessions.get_wallet_manager( token )->sign_digest( public_key, sig_digest );
}

signature_type beekeeper_wallet_manager::sign_transaction( const std::string& token, const string& transaction, const chain_id_type& chain_id, const public_key_type& public_key, const digest_type& sig_digest )
{
  sessions.check_timeout( token );
  return sessions.get_wallet_manager( token )->sign_transaction( transaction, chain_id, public_key, sig_digest );
}

info beekeeper_wallet_manager::get_info( const std::string& token )
{
  return sessions.get_info( token );
}

void beekeeper_wallet_manager::save_connection_details( const collector_t& values )
{
  singleton->save_connection_details( values );
}

string beekeeper_wallet_manager::create_session( const string& salt, const string& notifications_endpoint )
{
  FC_ASSERT( session_cnt < session_limit, "Number of concurrent sessions reached a limit ==`${session_limit}`. Close previous sessions so as to open the new one.", (session_limit) );

  auto _token = sessions.create_session( salt, notifications_endpoint );
  set_timeout( _token, unlock_timeout );

  ++session_cnt;

  return _token;
}

void beekeeper_wallet_manager::close_session( const string& token )
{
  sessions.close_session( token );
  if( sessions.empty() )
  {
    close_all_sessions_action();
  }

  --session_cnt;
}

} //beekeeper

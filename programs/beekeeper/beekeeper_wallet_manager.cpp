#include <beekeeper/beekeeper_wallet_manager.hpp>

#include <appbase/application.hpp>

namespace beekeeper {

bool beekeeper_wallet_manager::start( const boost::filesystem::path& command_line_wallet_dir, uint64_t command_line_unlock_timeout )
{
  unlock_timeout = command_line_unlock_timeout;

  wallet_impl = std::make_unique<wallet_manager_impl>( command_line_wallet_dir );

  FC_ASSERT( wallet_impl );
  return wallet_impl->start();
}

void beekeeper_wallet_manager::set_timeout( const std::string& token, uint64_t secs )
{
  sessions.set_timeout( token, std::chrono::seconds( secs ) );
}

std::string beekeeper_wallet_manager::create( const std::string& token, const std::string& name, fc::optional<std::string> password )
{
  sessions.check_timeout( token );

  FC_ASSERT( wallet_impl );
  return wallet_impl->create( token, name, password );
}

void beekeeper_wallet_manager::open( const std::string& token, const std::string& name )
{
  sessions.check_timeout( token );

  FC_ASSERT( wallet_impl );
  wallet_impl->open( token, name );
}

std::vector<wallet_details> beekeeper_wallet_manager::list_wallets( const std::string& token )
{
  sessions.check_timeout( token );

  FC_ASSERT( wallet_impl );
  return wallet_impl->list_wallets( token );
}

map<std::string, std::string> beekeeper_wallet_manager::list_keys( const std::string& token, const string& name, const string& pw )
{
  sessions.check_timeout( token );

  FC_ASSERT( wallet_impl );
  return wallet_impl->list_keys( token, name, pw );
}

flat_set<std::string> beekeeper_wallet_manager::get_public_keys( const std::string& token )
{
  sessions.check_timeout( token );

  FC_ASSERT( wallet_impl );
  return wallet_impl->get_public_keys( token );
}


void beekeeper_wallet_manager::lock_all( const std::string& token )
{
  FC_ASSERT( wallet_impl );
  wallet_impl->lock_all( token );
}

void beekeeper_wallet_manager::lock( const std::string& token, const std::string& name )
{
  sessions.check_timeout( token );

  FC_ASSERT( wallet_impl );
  wallet_impl->lock( token, name );
}

void beekeeper_wallet_manager::unlock( const std::string& token, const std::string& name, const std::string& password )
{
  sessions.check_timeout( token );

  FC_ASSERT( wallet_impl );
  wallet_impl->unlock( token, name, password );
}

string beekeeper_wallet_manager::import_key( const std::string& token, const std::string& name, const std::string& wif_key )
{
  sessions.check_timeout( token );

  FC_ASSERT( wallet_impl );
  return wallet_impl->import_key( token, name, wif_key );
}

void beekeeper_wallet_manager::remove_key( const std::string& token, const std::string& name, const std::string& password, const std::string& key )
{
  sessions.check_timeout( token );

  FC_ASSERT( wallet_impl );
  wallet_impl->remove_key( token, name, password, key );
}

string beekeeper_wallet_manager::create_key( const std::string& token, const std::string& name )
{
  sessions.check_timeout( token );

  FC_ASSERT( wallet_impl );
  return wallet_impl->create_key( token, name );
}

signature_type beekeeper_wallet_manager::sign_digest( const std::string& token, const digest_type& digest, const public_key_type& key )
{
  sessions.check_timeout( token );

  FC_ASSERT( wallet_impl );
  return wallet_impl->sign_digest( token, digest, key );
}

info beekeeper_wallet_manager::get_info( const std::string& token )
{
  return sessions.get_info( token );
}

void beekeeper_wallet_manager::save_connection_details( const collector_t& values )
{
  FC_ASSERT( wallet_impl );
  wallet_impl->save_connection_details( values );
}

string beekeeper_wallet_manager::create_session( const string& salt, const string& notifications_endpoint )
{
  auto _token = sessions.create_session( salt, notifications_endpoint, [this]( const std::string& token ){ lock_all( token ); } );
  set_timeout( _token, unlock_timeout );

  return _token;
}

void beekeeper_wallet_manager::close_session( const string& token )
{
  if( sessions.close_session( token ) )
  {
    //Close beekeeper, because there aren't any sessions.
    std::raise(SIGINT);
  }
}

} //beekeeper

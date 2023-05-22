#include <beekeeper/beekeeper_wallet_manager.hpp>

#include <appbase/application.hpp>

namespace beekeeper {

beekeeper_wallet_manager::beekeeper_wallet_manager( const boost::filesystem::path& command_line_wallet_dir, uint64_t command_line_unlock_timeout, close_all_sessions_action_method method )
                          : close_all_sessions_action( method )
{
  unlock_timeout = command_line_unlock_timeout;

  singleton = std::make_unique<singleton_beekeeper>( command_line_wallet_dir );
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

  return sessions.get_wallet( token )->create( [this]( const std::string& name ){ return singleton->create_wallet_filename( name ); }, name, password );
}

void beekeeper_wallet_manager::open( const std::string& token, const std::string& name )
{
  sessions.check_timeout( token );

  sessions.get_wallet( token )->open( [this]( const std::string& name ){ return singleton->create_wallet_filename( name ); }, name );
}

std::vector<wallet_details> beekeeper_wallet_manager::list_wallets( const std::string& token )
{
  sessions.check_timeout( token );
  return sessions.get_wallet( token )->list_wallets();
}

map<std::string, std::string> beekeeper_wallet_manager::list_keys( const std::string& token, const string& name, const string& pw )
{
  sessions.check_timeout( token );
  return sessions.get_wallet( token )->list_keys( name, pw );
}

flat_set<std::string> beekeeper_wallet_manager::get_public_keys( const std::string& token )
{
  sessions.check_timeout( token );
  return sessions.get_wallet( token )->get_public_keys();
}

void beekeeper_wallet_manager::lock_all( const std::string& token )
{
  sessions.get_wallet( token )->lock_all();
}

void beekeeper_wallet_manager::lock( const std::string& token, const std::string& name )
{
  sessions.check_timeout( token );
  sessions.get_wallet( token )->lock( name );
}

void beekeeper_wallet_manager::unlock( const std::string& token, const std::string& name, const std::string& password )
{
  sessions.check_timeout( token );
  sessions.get_wallet( token )->unlock( [this]( const std::string& name ){ return singleton->create_wallet_filename( name ); }, name, password );
}

string beekeeper_wallet_manager::import_key( const std::string& token, const std::string& name, const std::string& wif_key )
{
  sessions.check_timeout( token );
  return sessions.get_wallet( token )->import_key( name, wif_key );
}

void beekeeper_wallet_manager::remove_key( const std::string& token, const std::string& name, const std::string& password, const std::string& key )
{
  sessions.check_timeout( token );
  sessions.get_wallet( token )->remove_key( name, password, key );
}

string beekeeper_wallet_manager::create_key( const std::string& token, const std::string& name )
{
  sessions.check_timeout( token );
  return sessions.get_wallet( token )->create_key( name );
}

signature_type beekeeper_wallet_manager::sign_digest( const std::string& token, const digest_type& digest, const public_key_type& key )
{
  sessions.check_timeout( token );
  return sessions.get_wallet( token )->sign_digest( digest, key );
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
  auto _token = sessions.create_session( salt, notifications_endpoint, [this]( const std::string& token ){ lock_all( token ); } );
  set_timeout( _token, unlock_timeout );

  return _token;
}

void beekeeper_wallet_manager::close_session( const string& token )
{
  sessions.close_session( token );
  if( sessions.empty() )
  {
    close_all_sessions_action();
  }
}

} //beekeeper

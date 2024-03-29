#include <core/beekeeper_wallet_manager.hpp>

namespace beekeeper {

beekeeper_wallet_manager::beekeeper_wallet_manager( std::shared_ptr<session_manager_base> sessions, std::shared_ptr<beekeeper_instance_base> instance, const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit,
                                                    close_all_sessions_action_method&& method
                                                  )
                          : unlock_timeout( cmd_unlock_timeout ), session_limit( cmd_session_limit ),
                            public_key_size( private_key_type::generate().get_public_key().to_base58().size() ),
                            wallet_directory( cmd_wallet_dir ),
                            close_all_sessions_action( method ),
                            sessions( sessions ), instance( instance )
{
}

bool beekeeper_wallet_manager::start()
{
  return instance->start();
}

void beekeeper_wallet_manager::set_timeout( const std::string& token, seconds_type seconds )
{
  set_timeout_impl( token, seconds );
}

void beekeeper_wallet_manager::set_timeout_impl( const std::string& token, seconds_type seconds )
{
  sessions->set_timeout( token, std::chrono::seconds( seconds ) );
}

std::string beekeeper_wallet_manager::create( const std::string& token, const std::string& name, const std::optional<std::string>& password )
{
  sessions->check_timeout( token );

  return sessions->get_wallet_manager( token )->create( name, password );
}

void beekeeper_wallet_manager::open( const std::string& token, const std::string& name )
{
  sessions->check_timeout( token );

  sessions->get_wallet_manager( token )->open( name );
}

void beekeeper_wallet_manager::close( const std::string& token, const std::string& name )
{
  sessions->check_timeout( token );

  sessions->get_wallet_manager( token )->close( name );
}

std::vector<wallet_details> beekeeper_wallet_manager::list_wallets( const std::string& token )
{
  sessions->check_timeout( token );
  return sessions->get_wallet_manager( token )->list_wallets();
}

std::vector<wallet_details> beekeeper_wallet_manager::list_created_wallets(const std::string& token)
{
  sessions->check_timeout(token);

  return sessions->get_wallet_manager(token)->list_created_wallets();
}

map<public_key_type, private_key_type> beekeeper_wallet_manager::list_keys( const std::string& token, const string& name, const string& pw )
{
  sessions->check_timeout( token );
  return sessions->get_wallet_manager( token )->list_keys( name, pw );
}

flat_set<public_key_type> beekeeper_wallet_manager::get_public_keys( const std::string& token, const std::optional<std::string>& wallet_name )
{
  sessions->check_timeout( token );
  return sessions->get_wallet_manager( token )->get_public_keys( wallet_name );
}

void beekeeper_wallet_manager::lock_all( const std::string& token )
{
  sessions->get_wallet_manager( token )->lock_all();
}

void beekeeper_wallet_manager::lock( const std::string& token, const std::string& name )
{
  sessions->get_wallet_manager( token )->lock( name );
}

void beekeeper_wallet_manager::unlock( const std::string& token, const std::string& name, const std::string& password )
{
  sessions->get_wallet_manager( token )->unlock( name, password );
}

string beekeeper_wallet_manager::import_key( const std::string& token, const std::string& name, const std::string& wif_key )
{
  sessions->check_timeout( token );
  return sessions->get_wallet_manager( token )->import_key( name, wif_key );
}

void beekeeper_wallet_manager::remove_key( const std::string& token, const std::string& name, const std::string& password, const std::string& public_key )
{
  sessions->check_timeout( token );
  sessions->get_wallet_manager( token )->remove_key( name, password, public_key );
}

signature_type beekeeper_wallet_manager::sign_digest( const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::optional<std::string>& wallet_name )
{
  FC_ASSERT( sig_digest.size(), "`sig_digest` can't be empty" );

  sessions->check_timeout( token );
  return sessions->get_wallet_manager( token )->sign_digest( digest_type( sig_digest ), create_public_key( public_key ), wallet_name );
}

info beekeeper_wallet_manager::get_info( const std::string& token )
{
  return sessions->get_info( token );
}

string beekeeper_wallet_manager::create_session( const string& salt, const string& notifications_endpoint )
{
  FC_ASSERT( session_cnt < session_limit, "Number of concurrent sessions reached a limit ==`${session_limit}`. Close previous sessions so as to open the new one.", (session_limit) );

  auto _token = sessions->create_session( salt, notifications_endpoint, wallet_directory );
  set_timeout_impl( _token, unlock_timeout );

  ++session_cnt;

  return _token;
}

void beekeeper_wallet_manager::close_session( const string& token, bool allow_close_all_sessions_action )
{
  sessions->close_session( token );
  if( allow_close_all_sessions_action && sessions->empty() )
  {
    close_all_sessions_action();
  }

  --session_cnt;
}

bool beekeeper_wallet_manager::has_matching_private_key( const std::string& token, const std::string& name, const std::string& public_key )
{
  sessions->check_timeout( token );
  return sessions->get_wallet_manager( token )->has_matching_private_key( name, create_public_key( public_key ) );
}

public_key_type beekeeper_wallet_manager::create_public_key( const std::string& public_key )
{
  FC_ASSERT( public_key_size == public_key.size(), "Incorrect size of public key. Expected ${public_key_size}, but got ${public_key_size_2}",
                                                    (public_key_size)("public_key_size_2", public_key.size()) );

  return utility::public_key::create( public_key );
}

std::string beekeeper_wallet_manager::encrypt_data( const std::string& token, const std::string& from_public_key, const std::string& to_public_key, const std::string& wallet_name, const std::string& content )
{
  sessions->check_timeout( token );
  return sessions->get_wallet_manager( token )->encrypt_data( create_public_key( from_public_key ), create_public_key( to_public_key ), wallet_name, content );
}

std::string beekeeper_wallet_manager::decrypt_data( const std::string& token, const std::string& from_public_key, const std::string& to_public_key, const std::string& wallet_name, const std::string& encrypted_content )
{
  sessions->check_timeout( token );
  return sessions->get_wallet_manager( token )->decrypt_data( create_public_key( from_public_key ), create_public_key( to_public_key ), wallet_name, encrypted_content );
}

} //beekeeper

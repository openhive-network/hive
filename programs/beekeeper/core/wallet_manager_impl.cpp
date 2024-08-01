#include <core/wallet_manager_impl.hpp>
#include <core/beekeeper_wallet.hpp>

#include <fc/filesystem.hpp>
#include <fc/crypto/crypto_data.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

namespace beekeeper {

namespace bfs = boost::filesystem;

std::string wallet_manager_impl::gen_password()
{
  constexpr auto password_prefix = "PW";

  auto key = private_key_type::generate();
  return password_prefix + key.key_to_wif();
}

void wallet_manager_impl::valid_filename( const std::string& name )
{
  FC_ASSERT( !name.empty(), "Name of wallet is incorrect. Is empty.");

  FC_ASSERT( std::find_if(name.begin(), name.end(), !boost::algorithm::is_alnum() && !boost::algorithm::is_any_of("._-@")) == name.end(),
        "Name of wallet is incorrect. Name: ${name}. Only alphanumeric and '._-@' chars are allowed", (name) );

  FC_ASSERT( bfs::path(name).filename().string() == name,
          "Name of wallet is incorrect. Name: ${name}. File creation with given name is impossible.", (name) );
}

std::string wallet_manager_impl::create( const std::string& wallet_name, const std::optional<std::string>& password )
{
  FC_ASSERT( !password || password->size() < max_password_length,
            "Got ${password_size} bytes, but a password limit has ${max_password_length} bytes", ("password_size", password->size())(max_password_length) );

  valid_filename( wallet_name );

  auto wallet_filename = create_wallet_filename( wallet_name );
  FC_ASSERT( !bfs::exists(wallet_filename), "Wallet with name: '${n}' already exists at ${path}", ("n", wallet_name)("path", fc::path(wallet_filename)) );

  std::string _password = password ? ( *password ) : gen_password();

  auto wallet = std::make_unique<beekeeper_wallet>();
  wallet->set_password( _password );
  wallet->set_wallet_filename(wallet_filename.string());
  wallet->unlock( _password );
  wallet->lock();
  wallet->unlock( _password );

  // Explicitly save the wallet file here, to ensure it now exists.
  wallet->save_wallet_file();

  // If we have name in our map then remove it since we want the emplace below to replace.
  // This can happen if the wallet file is removed while a wallet is running.
  auto it = wallets.find( wallet_name );
  if (it != wallets.end())
  {
    wallets.erase(it);
  }
  wallets.emplace( wallet_name, std::move(wallet) );

  return _password;
}

void wallet_manager_impl::open( const std::string& wallet_name )
{
  valid_filename( wallet_name );

  auto wallet = std::make_unique<beekeeper_wallet>();
  auto wallet_filename = create_wallet_filename( wallet_name );
  wallet->set_wallet_filename(wallet_filename.string());
  FC_ASSERT( wallet->load_wallet_file(), "Unable to open file: ${f}", ("f", wallet_filename.string()));

  // If we have name in our map then remove it since we want the emplace below to replace.
  // This can happen if the wallet file is added while a wallet is running.
  auto it = wallets.find( wallet_name );
  if (it != wallets.end())
  {
    wallets.erase(it);
  }
  wallets.emplace( wallet_name, std::move(wallet) );
}

void wallet_manager_impl::close( const std::string& wallet_name )
{
  FC_ASSERT( wallets.count( wallet_name ), "Wallet not found: ${w}", ("w", wallet_name));

  if( !is_locked( wallet_name ) )
    lock( wallet_name );

  wallets.erase( wallet_name );
}

fc::optional<private_key_type> wallet_manager_impl::find_private_key_in_given_wallet( const public_key_type& public_key, const std::string& wallet_name )
{
  keys_details _keys = list_keys_impl( wallet_name, std::string(), false/*password_is_required*/ );
  for( auto& key : _keys )
  {
    if( key.first == public_key )
      return key.second.first;
  }

  return fc::optional<private_key_type>();
}

std::vector<wallet_details> wallet_manager_impl::list_wallets_impl( const std::vector< std::string >& wallet_files )
{
  std::vector<wallet_details> _result;

  for(const auto& wallet_file_name : wallet_files )
  {
    auto it = wallets.find(wallet_file_name);
    /// For each not opened wallet perform implicit open - this is needed to correctly support a close method
    if( it == wallets.end() )
      open( wallet_file_name );
  }

  for(const auto& w : wallets)
  {
    _result.emplace_back( wallet_details{ w.first, !w.second->is_locked() } );
  }

  return _result;
}

std::vector< std::string > wallet_manager_impl::list_created_wallets_impl( const boost::filesystem::path& directory, const std::string& extension ) const
{
  std::vector< std::string > _result;

  boost::filesystem::directory_iterator _end_itr;
  for( boost::filesystem::directory_iterator itr( directory ); itr != _end_itr; ++itr )
  {
    if( boost::filesystem::is_regular_file( itr->path() ) )
    {
      if( itr->path().extension() == extension )
      {
        std::vector<std::string> _path_parts;
        boost::split( _path_parts, itr->path().c_str(), boost::is_any_of("/") );
        if( !_path_parts.empty() )
        {
          auto _end = *_path_parts.rbegin();
          auto _found = _end.find( extension );
          if( _found != std::string::npos ) 
              _result.emplace_back( _end.substr( 0, _found ) );
        }
      }
    }
  }
  return _result;
}

std::vector<wallet_details> wallet_manager_impl::list_wallets()
{
  return list_wallets_impl( std::vector< std::string >() );
}

std::vector<wallet_details> wallet_manager_impl::list_created_wallets()
{
  return list_wallets_impl( list_created_wallets_impl( get_wallet_directory(), get_extension() ) );
}

keys_details wallet_manager_impl::list_keys_impl( const std::string& name, const std::string& password, bool password_is_required )
{
  FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));
  auto& w = wallets.at(name);
  FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));
  if( password_is_required )
    w->check_password( password ); //throws if bad password
  return w->list_keys();
}

keys_details wallet_manager_impl::list_keys( const std::string& name, const std::string& password )
{
  return list_keys_impl( name, password, true/*password_is_required*/ );
}

keys_details wallet_manager_impl::get_public_keys( const std::optional<std::string>& wallet_name )
{
  FC_ASSERT( !wallets.empty(), "You don't have any wallet");

  keys_details _result;
  bool is_all_wallet_locked = true;

  auto _process_wallet = [&]( const std::unique_ptr<beekeeper_wallet>& wallet )
  {
    if( !wallet->is_locked() )
    {
      _result.merge( wallet->list_public_keys() );
    }
    is_all_wallet_locked &= wallet->is_locked();
  };

  if( wallet_name )
  {
    auto _it = wallets.find( *wallet_name );
    if( _it != wallets.end() )
      _process_wallet( _it->second );
  }
  else
  {
    for( const auto& i : wallets )
      _process_wallet( i.second );
  }

  if( wallet_name )
    FC_ASSERT( !is_all_wallet_locked, "Wallet ${wallet_name} is locked", ("wallet_name", *wallet_name));
  else
    FC_ASSERT( !is_all_wallet_locked, "You don't have any unlocked wallet");

  return _result;
}

void wallet_manager_impl::lock_all()
{
  // no call to check_timeout since we are locking all anyway
  for (auto& i : wallets)
  {
    if (!i.second->is_locked())
    {
      i.second->lock();
    }
  }
}

bool wallet_manager_impl::is_locked( const std::string& name )
{
  FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));
  auto& w = wallets.at(name);

  return w->is_locked();
}

void wallet_manager_impl::lock( const std::string& wallet_name )
{
  FC_ASSERT( wallets.count( wallet_name ), "Wallet not found: ${w}", ("w", wallet_name));
  auto& w = wallets.at( wallet_name );

  w->lock();
}

void wallet_manager_impl::unlock( const std::string& wallet_name, const std::string& password )
{
  if( wallets.count( wallet_name ) == 0 )
  {
    open( wallet_name );
  }
  auto& w = wallets.at( wallet_name );
  FC_ASSERT( w->is_locked(), "Wallet is already unlocked: ${w}", ("w", wallet_name));

  w->unlock( password );
}

std::string wallet_manager_impl::import_key( const std::string& name, const std::string& wif_key, const std::string& prefix )
{
  FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

  auto& w = wallets.at(name);
  FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));

  return w->import_key( wif_key, prefix );
}

std::vector<std::string> wallet_manager_impl::import_keys( const std::string& name, const std::vector<std::string>& wif_keys, const std::string& prefix )
{
  FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

  auto& w = wallets.at(name);
  FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));

  return w->import_keys( wif_keys, prefix );
}

void wallet_manager_impl::remove_key( const std::string& name, const public_key_type& public_key )
{
  FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

  auto& w = wallets.at(name);
  FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));

  w->remove_key( public_key );
}

signature_type wallet_manager_impl::sign( std::function<std::optional<signature_type>(const std::unique_ptr<beekeeper_wallet>&)>&& sign_method, const std::optional<std::string>& wallet_name, const public_key_type& public_key, const std::string& prefix )
{
  try
  {
    auto _process_wallet = [&]( const std::unique_ptr<beekeeper_wallet>& wallet )
    {
      if( !wallet->is_locked() )
        return sign_method( wallet );
      return std::optional<signature_type>();
    };

    if( wallet_name )
    {
      auto _it = wallets.find( *wallet_name );
      if( _it != wallets.end() )
      {
        auto _sig = _process_wallet( _it->second );
        if( _sig )
          return *_sig;
      }
    }
    else
    {
      for( const auto& i : wallets )
      {
        auto _sig = _process_wallet( i.second );
        if( _sig )
          return *_sig;
      }
    }
  } FC_LOG_AND_RETHROW();

  if( wallet_name )
    FC_ASSERT( false, "Public key ${public_key} not found in ${wallet} wallet", ("wallet", *wallet_name)("public_key", utility::public_key::to_string( public_key, prefix )));
  else
    FC_ASSERT( false, "Public key ${public_key} not found in unlocked wallets", ("public_key", utility::public_key::to_string( public_key, prefix )));
}

signature_type wallet_manager_impl::sign_digest( const std::optional<std::string>& wallet_name, const digest_type& sig_digest, const public_key_type& public_key, const std::string& prefix )
{
  return sign( [&]( const std::unique_ptr<beekeeper_wallet>& wallet ){ return wallet->try_sign_digest( sig_digest, public_key ); }, wallet_name, public_key, prefix );
}

bool wallet_manager_impl::has_matching_private_key( const std::string& wallet_name, const public_key_type& public_key )
{
  FC_ASSERT( wallets.count( wallet_name ), "Wallet not found: ${w}", ("w", wallet_name));

  auto& w = wallets.at( wallet_name );
  FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", wallet_name));

  return w->has_matching_private_key( public_key );
}

std::string wallet_manager_impl::encrypt_data( const public_key_type& from_public_key, const public_key_type& to_public_key, const std::string& wallet_name, const std::string& content, const std::optional<unsigned int>& nonce, const std::string& prefix )
{
  fc::crypto_data _cd;

  auto _private_key = find_private_key_in_given_wallet( from_public_key, wallet_name );
  if( !_private_key )
    FC_ASSERT( false, "Public key ${public_key} not found in ${wallet} wallet", ("wallet", wallet_name)("public_key", utility::public_key::to_string( from_public_key, prefix )));

  return _cd.encrypt( _private_key.value(), to_public_key, content, nonce );
}

std::string wallet_manager_impl::decrypt_data( const public_key_type& from_public_key, const public_key_type& to_public_key, const std::string& wallet_name, const std::string& encrypted_content )
{
  fc::crypto_data _cd;

  return _cd.decrypt
  (
    [&wallet_name, this]( const public_key_type& key )
    {
      return find_private_key_in_given_wallet( key, wallet_name );
    },
    from_public_key, to_public_key,
    encrypted_content
  );
}

} //beekeeper

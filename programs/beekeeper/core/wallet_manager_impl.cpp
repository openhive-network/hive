#include <core/wallet_manager_impl.hpp>
#include <core/beekeeper_wallet.hpp>

#include <fc/filesystem.hpp>

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

void wallet_manager_impl::valid_filename( const string& name )
{
  FC_ASSERT( !name.empty(), "Name of wallet is incorrect. Is empty.");

  FC_ASSERT( std::find_if(name.begin(), name.end(), !boost::algorithm::is_alnum() && !boost::algorithm::is_any_of("._-")) == name.end(),
        "Name of wallet is incorrect. Name: ${name}. Only alphanumeric and '._-' chars are allowed", (name) );

  FC_ASSERT( bfs::path(name).filename().string() == name,
          "Name of wallet is incorrect. Name: ${name}. File creation with given name is impossible.", (name) );
}

std::string wallet_manager_impl::create( wallet_filename_creator_type wallet_filename_creator, const std::string& name, const std::optional<std::string>& password )
{
  FC_ASSERT( !password || password->size() < max_password_length,
            "Got ${password_size} bytes, but a password limit has ${max_password_length} bytes", ("password_size", password->size())(max_password_length) );

  valid_filename(name);

  auto wallet_filename = wallet_filename_creator( name );
  FC_ASSERT( !bfs::exists(wallet_filename), "Wallet with name: '${n}' already exists at ${path}", ("n", name)("path", fc::path(wallet_filename)) );

  std::string _password = password ? ( *password ) : gen_password();

  wallet_data d;
  auto wallet = make_unique<beekeeper_wallet>(d);
  wallet->set_password( _password );
  wallet->set_wallet_filename(wallet_filename.string());
  wallet->unlock( _password );
  wallet->lock();
  wallet->unlock( _password );

  // Explicitly save the wallet file here, to ensure it now exists.
  wallet->save_wallet_file();

  // If we have name in our map then remove it since we want the emplace below to replace.
  // This can happen if the wallet file is removed while a wallet is running.
  auto it = wallets.find(name);
  if (it != wallets.end())
  {
    wallets.erase(it);
  }
  wallets.emplace(name, std::move(wallet));

  return _password;
}

void wallet_manager_impl::open( wallet_filename_creator_type wallet_filename_creator, const std::string& name )
{
  valid_filename(name);

  wallet_data d;
  auto wallet = std::make_unique<beekeeper_wallet>(d);
  auto wallet_filename = wallet_filename_creator( name );
  wallet->set_wallet_filename(wallet_filename.string());
  FC_ASSERT( wallet->load_wallet_file(), "Unable to open file: ${f}", ("f", wallet_filename.string()));

  // If we have name in our map then remove it since we want the emplace below to replace.
  // This can happen if the wallet file is added while a wallet is running.
  auto it = wallets.find(name);
  if (it != wallets.end())
  {
    wallets.erase(it);
  }
  wallets.emplace(name, std::move(wallet));
}

void wallet_manager_impl::close( const std::string& name )
{
  FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

  if( !is_locked( name ) )
    lock( name );

  wallets.erase( name );
}

std::vector<wallet_details> wallet_manager_impl::list_wallets()
{
  std::vector<wallet_details> result;
  for (const auto& i : wallets)
  {
    result.emplace_back( wallet_details{ i.first, !i.second->is_locked() } );
  }
  return result;
}

map<public_key_type, private_key_type> wallet_manager_impl::list_keys( const string& name, const string& pw )
{
  FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));
  auto& w = wallets.at(name);
  FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));
  w->check_password(pw); //throws if bad password
  return w->list_keys();
}

flat_set<public_key_type> wallet_manager_impl::get_public_keys()
{
  FC_ASSERT( !wallets.empty(), "You don't have any wallet");

  flat_set<public_key_type> result;
  bool is_all_wallet_locked = true;

  for( const auto& i : wallets )
  {
    if( !i.second->is_locked() )
    {
      result.merge( i.second->list_public_keys() );
    }
    is_all_wallet_locked &= i.second->is_locked();
  }

  FC_ASSERT( !is_all_wallet_locked, "You don't have any unlocked wallet");
  return result;
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

bool wallet_manager_impl::is_locked( const string& name )
{
  FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));
  auto& w = wallets.at(name);

  return w->is_locked();
}

void wallet_manager_impl::lock( const std::string& name )
{
  FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));
  auto& w = wallets.at(name);

  w->lock();
}

void wallet_manager_impl::unlock( wallet_filename_creator_type wallet_filename_creator, const std::string& name, const std::string& password )
{
  if (wallets.count(name) == 0)
  {
    open( wallet_filename_creator, name );
  }
  auto& w = wallets.at(name);
  FC_ASSERT( w->is_locked(), "Wallet is already unlocked: ${w}", ("w", name));

  w->unlock(password);
}

string wallet_manager_impl::import_key( const std::string& name, const std::string& wif_key )
{
  FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

  auto& w = wallets.at(name);
  FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));

  return w->import_key(wif_key);
}

void wallet_manager_impl::remove_key( const std::string& name, const std::string& password, const std::string& public_key )
{
  FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

  auto& w = wallets.at(name);
  FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));

  w->check_password(password); //throws if bad password
  w->remove_key(public_key);
}

signature_type wallet_manager_impl::sign( std::function<std::optional<signature_type>(const std::unique_ptr<beekeeper_wallet_base>&)>&& sign_method, const public_key_type& public_key )
{
  try
  {
    for( const auto& i : wallets )
    {
      if( !i.second->is_locked() )
      {
        std::optional<signature_type> sig = sign_method( i.second );
        if( sig )
          return *sig;
      }
    }
  } FC_LOG_AND_RETHROW();

  FC_ASSERT( false, "Public key not found in unlocked wallets ${public_key}", (utility::public_key::to_string( public_key )));
}

signature_type wallet_manager_impl::sign_digest( const digest_type& sig_digest, const public_key_type& public_key )
{
  return sign( [&]( const std::unique_ptr<beekeeper_wallet_base>& wallet ){ return wallet->try_sign_digest( sig_digest, public_key ); }, public_key );
}

} //beekeeper

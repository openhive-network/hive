#include <beekeeper/beekeeper_wallet_manager.hpp>
#include <beekeeper/beekeeper_wallet.hpp>

#include <appbase/application.hpp>

#include <fc/filesystem.hpp>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

namespace beekeeper {

namespace bfs = boost::filesystem;

constexpr auto password_prefix = "PW";

std::string gen_password()
{
   auto key = private_key_type::generate();
   return password_prefix + key.key_to_wif();
}

bool valid_filename(const string& name)
{
   if (name.empty()) return false;
   if (std::find_if(name.begin(), name.end(), !boost::algorithm::is_alnum() && !boost::algorithm::is_any_of("._-")) != name.end()) return false;
   return bfs::path(name).filename().string() == name;
}

beekeeper_wallet_manager::beekeeper_wallet_manager()
{
}

beekeeper_wallet_manager::~beekeeper_wallet_manager()
{
}

bool beekeeper_wallet_manager::start( const boost::filesystem::path& p )
{
  singleton = std::make_unique<singleton_beekeeper>( p );
  return singleton->start();
}

void beekeeper_wallet_manager::set_timeout( const std::string& token, const std::chrono::seconds& t )
{
  sessions.set_timeout( t, token );
}

std::string beekeeper_wallet_manager::create( const std::string& token, const std::string& name, fc::optional<std::string> password )
{
   sessions.check_timeout( token );

   FC_ASSERT( valid_filename(name), "Invalid filename, path not allowed in wallet name ${n}", ("n", name));
   FC_ASSERT( singleton );

   auto wallet_filename = singleton->create_wallet_filename( name );

   FC_ASSERT( !fc::exists(wallet_filename), "Wallet with name: '${n}' already exists at ${path}", ("n", name)("path",fc::path(wallet_filename)));

   if(!password)
      password = gen_password();

   wallet_data d;
   auto wallet = make_unique<beekeeper_wallet>(d);
   wallet->set_password(*password);
   wallet->set_wallet_filename(wallet_filename.string());
   wallet->unlock(*password);
   wallet->lock();
   wallet->unlock(*password);

   // Explicitly save the wallet file here, to ensure it now exists.
   wallet->save_wallet_file();

   // If we have name in our map then remove it since we want the emplace below to replace.
   // This can happen if the wallet file is removed while eos-walletd is running.
   auto it = wallets.find(name);
   if (it != wallets.end())
   {
      wallets.erase(it);
   }
   wallets.emplace(name, std::move(wallet));

   return *password;
}

void beekeeper_wallet_manager::open( const std::string& token, const std::string& name )
{
   sessions.check_timeout( token );

   FC_ASSERT( valid_filename(name), "Invalid filename, path not allowed in wallet name ${n}", ("n", name));
   FC_ASSERT( singleton );

   wallet_data d;
   auto wallet = std::make_unique<beekeeper_wallet>(d);
   auto wallet_filename = singleton->create_wallet_filename( name );
   wallet->set_wallet_filename(wallet_filename.string());
   FC_ASSERT( wallet->load_wallet_file(), "Unable to open file: ${f}", ("f", wallet_filename.string()));

   // If we have name in our map then remove it since we want the emplace below to replace.
   // This can happen if the wallet file is added while eos-walletd is running.
   auto it = wallets.find(name);
   if (it != wallets.end())
   {
      wallets.erase(it);
   }
   wallets.emplace(name, std::move(wallet));
}

std::vector<wallet_details> beekeeper_wallet_manager::list_wallets( const std::string& token )
{
   sessions.check_timeout( token );

   std::vector<wallet_details> result;
   for (const auto& i : wallets)
   {
      result.emplace_back( wallet_details{ i.first, !i.second->is_locked() } );
   }
   return result;
}

map<std::string, std::string> beekeeper_wallet_manager::list_keys( const std::string& token, const string& name, const string& pw )
{
   sessions.check_timeout( token );

   FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));
   auto& w = wallets.at(name);
   FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));
   w->check_password(pw); //throws if bad password
   return w->list_keys();
}

flat_set<std::string> beekeeper_wallet_manager::get_public_keys( const std::string& token )
{
   sessions.check_timeout( token );

   FC_ASSERT( !wallets.empty(), "You don't have any wallet!");
   flat_set<std::string> result;
   bool is_all_wallet_locked = true;
   for (const auto& i : wallets)
   {
      if (!i.second->is_locked())
      {
         result.merge(i.second->list_public_keys());
      }
      is_all_wallet_locked &= i.second->is_locked();
   }
   FC_ASSERT( !is_all_wallet_locked, "You don't have any unlocked wallet!");
   return result;
}


void beekeeper_wallet_manager::lock_all( const std::string& token )
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

void beekeeper_wallet_manager::lock( const std::string& token, const std::string& name )
{
   sessions.check_timeout( token );

   FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));
   auto& w = wallets.at(name);
   if (w->is_locked())
   {
      return;
   }
   w->lock();
}

void beekeeper_wallet_manager::unlock( const std::string& token, const std::string& name, const std::string& password )
{
   sessions.check_timeout( token );

   if (wallets.count(name) == 0)
   {
      open( token, name );
   }
   auto& w = wallets.at(name);
   FC_ASSERT( w->is_locked(), "Wallet is already unlocked: ${w}", ("w", name));

   w->unlock(password);
}

string beekeeper_wallet_manager::import_key( const std::string& token, const std::string& name, const std::string& wif_key )
{
   sessions.check_timeout( token );

   FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

   auto& w = wallets.at(name);
   FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));

   return w->import_key(wif_key);
}

void beekeeper_wallet_manager::remove_key( const std::string& token, const std::string& name, const std::string& password, const std::string& key )
{
   sessions.check_timeout( token );

   FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

   auto& w = wallets.at(name);
   FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));

   w->check_password(password); //throws if bad password
   w->remove_key(key);
}

string beekeeper_wallet_manager::create_key( const std::string& token, const std::string& name )
{
   sessions.check_timeout( token );

   FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

   auto& w = wallets.at(name);
   FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));

   return w->create_key();
}

signature_type beekeeper_wallet_manager::sign_digest( const std::string& token, const digest_type& digest, const public_key_type& key )
{
   sessions.check_timeout( token );

   try {
      for (const auto& i : wallets)
      {
         if (!i.second->is_locked())
         {
            std::optional<signature_type> sig = i.second->try_sign_digest(digest, key);
            if (sig)
               return *sig;
         }
      }
   } FC_LOG_AND_RETHROW();

   FC_ASSERT( false, "Public key not found in unlocked wallets ${k}", ("k", key));
}

info beekeeper_wallet_manager::get_info( const std::string& token )
{
   return sessions.get_info( token );
}

string beekeeper_wallet_manager::create_session( const string& salt, const string& notifications_endpoint )
{
  return sessions.create_session( salt, notifications_endpoint, [this]( const std::string& token ){ lock_all( token ); } );
}

void beekeeper_wallet_manager::close_session( const string& token )
{
  sessions.close_session( token );
}

void beekeeper_wallet_manager::save_connection_details( const collector_t& values )
{
   FC_ASSERT( singleton );
   singleton->save_connection_details( values );
}

} //beekeeper

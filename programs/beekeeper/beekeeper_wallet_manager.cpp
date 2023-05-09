#include <beekeeper/beekeeper_wallet_manager.hpp>
#include <beekeeper/beekeeper_wallet.hpp>

#include <appbase/application.hpp>

#include <fc/filesystem.hpp>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

namespace beekeeper {

namespace bfs = boost::filesystem;

constexpr auto file_ext = ".wallet";
constexpr auto password_prefix = "PW";

std::string gen_password() {
   auto key = private_key_type::generate();
   return password_prefix + key.key_to_wif();
}

bool valid_filename(const string& name) {
   if (name.empty()) return false;
   if (std::find_if(name.begin(), name.end(), !boost::algorithm::is_alnum() && !boost::algorithm::is_any_of("._-")) != name.end()) return false;
   return bfs::path(name).filename().string() == name;
}

time_manager::time_manager( method_type&& lock_method )
               :  lock_method( lock_method ),
                  notification_method( [](){ hive::notify_hived_status("Attempt of closing all wallets"); } )
{
   notification_thread = std::make_unique<std::thread>( [this]()
      {
         while( !stop_requested )
         {
            check_timeout_impl( false/*allow_update_timeout_time*/ );
            std::this_thread::sleep_for( std::chrono::milliseconds(200) );
         }
      } );
}

time_manager::~time_manager()
{
   stop_requested = true;
   notification_thread->join();
}

void time_manager::set_timeout( const std::chrono::seconds& t )
{
   timeout = t;
   auto now = std::chrono::system_clock::now();
   timeout_time = now + timeout;
   FC_ASSERT( timeout_time >= now && timeout_time.time_since_epoch().count() > 0, "Overflow on timeout_time, specified ${t}, now ${now}, timeout_time ${timeout_time}",
             ("t", t.count())("now", now.time_since_epoch().count())("timeout_time", timeout_time.time_since_epoch().count()));
}

void time_manager::check_timeout_impl( bool allow_update_timeout_time )
{
   if( timeout_time != timepoint_t::max() )
   {
      const auto& now = std::chrono::system_clock::now();
      if( now >= timeout_time )
      {
         {
            std::lock_guard<std::mutex> guard( methods_mutex );
            lock_method();
            notification_method();
            allow_update_timeout_time = true;
         }
      }
      if( allow_update_timeout_time )
         timeout_time = now + timeout;
   }
}

void time_manager::check_timeout()
{
   check_timeout_impl( true/*allow_update_timeout_time*/ );
}

info time_manager::get_info()
{
  auto to_string = []( const std::chrono::system_clock::time_point& tp )
  {
    fc::time_point_sec _time( tp.time_since_epoch() / std::chrono::milliseconds(1000) );
    return _time.to_iso_string();
  };

  return { to_string( std::chrono::system_clock::now() ), to_string( timeout_time ) };
}

beekeeper_wallet_manager::beekeeper_wallet_manager(): time( [this](){ lock_all(); } )
{
}

beekeeper_wallet_manager::~beekeeper_wallet_manager() {
   //not really required, but may spook users
   if(wallet_dir_lock)
      bfs::remove(lock_path);
}

void beekeeper_wallet_manager::set_timeout(const std::chrono::seconds& t)
{
   time.set_timeout( t );
}

std::string beekeeper_wallet_manager::create(const std::string& name, fc::optional<std::string> password) {
   time.check_timeout();

   FC_ASSERT( valid_filename(name), "Invalid filename, path not allowed in wallet name ${n}", ("n", name));

   auto wallet_filename = dir / (name + file_ext);

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
   if (it != wallets.end()) {
      wallets.erase(it);
   }
   wallets.emplace(name, std::move(wallet));

   return *password;
}

void beekeeper_wallet_manager::open(const std::string& name) {
   time.check_timeout();

   FC_ASSERT( valid_filename(name), "Invalid filename, path not allowed in wallet name ${n}", ("n", name));

   wallet_data d;
   auto wallet = std::make_unique<beekeeper_wallet>(d);
   auto wallet_filename = dir / (name + file_ext);
   wallet->set_wallet_filename(wallet_filename.string());
   FC_ASSERT( wallet->load_wallet_file(), "Unable to open file: ${f}", ("f", wallet_filename.string()));

   // If we have name in our map then remove it since we want the emplace below to replace.
   // This can happen if the wallet file is added while eos-walletd is running.
   auto it = wallets.find(name);
   if (it != wallets.end()) {
      wallets.erase(it);
   }
   wallets.emplace(name, std::move(wallet));
}

std::vector<wallet_details> beekeeper_wallet_manager::list_wallets() {
   time.check_timeout();
   std::vector<wallet_details> result;
   for (const auto& i : wallets)
   {
      result.emplace_back( wallet_details{ i.first, !i.second->is_locked() } );
   }
   return result;
}

map<std::string, std::string> beekeeper_wallet_manager::list_keys(const string& name, const string& pw) {
   time.check_timeout();

   FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));
   auto& w = wallets.at(name);
   FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));
   w->check_password(pw); //throws if bad password
   return w->list_keys();
}

flat_set<std::string> beekeeper_wallet_manager::get_public_keys() {
   time.check_timeout();
   FC_ASSERT( !wallets.empty(), "You don't have any wallet!");
   flat_set<std::string> result;
   bool is_all_wallet_locked = true;
   for (const auto& i : wallets) {
      if (!i.second->is_locked()) {
         result.merge(i.second->list_public_keys());
      }
      is_all_wallet_locked &= i.second->is_locked();
   }
   FC_ASSERT( !is_all_wallet_locked, "You don't have any unlocked wallet!");
   return result;
}


void beekeeper_wallet_manager::lock_all() {
   // no call to check_timeout since we are locking all anyway
   for (auto& i : wallets) {
      if (!i.second->is_locked()) {
         i.second->lock();
      }
   }
}

void beekeeper_wallet_manager::lock(const std::string& name) {
   time.check_timeout();
   FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));
   auto& w = wallets.at(name);
   if (w->is_locked()) {
      return;
   }
   w->lock();
}

void beekeeper_wallet_manager::unlock(const std::string& name, const std::string& password) {
   time.check_timeout();
   if (wallets.count(name) == 0) {
      open( name );
   }
   auto& w = wallets.at(name);
   FC_ASSERT( w->is_locked(), "Wallet is already unlocked: ${w}", ("w", name));

   w->unlock(password);
}

string beekeeper_wallet_manager::import_key(const std::string& name, const std::string& wif_key) {
   time.check_timeout();
   FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

   auto& w = wallets.at(name);
   FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));

   return w->import_key(wif_key);
}

void beekeeper_wallet_manager::remove_key(const std::string& name, const std::string& password, const std::string& key) {
   time.check_timeout();
   FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

   auto& w = wallets.at(name);
   FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));

   w->check_password(password); //throws if bad password
   w->remove_key(key);
}

string beekeeper_wallet_manager::create_key(const std::string& name) {
   time.check_timeout();
   FC_ASSERT( wallets.count(name), "Wallet not found: ${w}", ("w", name));

   auto& w = wallets.at(name);
   FC_ASSERT( !w->is_locked(), "Wallet is locked: ${w}", ("w", name));

   return w->create_key();
}

signature_type beekeeper_wallet_manager::sign_digest(const digest_type& digest, const public_key_type& key)
{
   time.check_timeout();

   try {
      for (const auto& i : wallets) {
         if (!i.second->is_locked()) {
            std::optional<signature_type> sig = i.second->try_sign_digest(digest, key);
            if (sig)
               return *sig;
         }
      }
   } FC_LOG_AND_RETHROW();

   FC_ASSERT( false, "Public key not found in unlocked wallets ${k}", ("k", key));
}

void beekeeper_wallet_manager::own_and_use_wallet(const string& name, std::unique_ptr<beekeeper_wallet_base>&& wallet) {
   FC_ASSERT( wallets.find(name) == wallets.end(), "Tried to use wallet name that already exists.");
   wallets.emplace(name, std::move(wallet));
}

void beekeeper_wallet_manager::start_lock_watch(std::shared_ptr<boost::asio::deadline_timer> t)
{
   t->async_wait([t, this](const boost::system::error_code& /*ec*/)
   {
      boost::system::error_code ec;
      auto rc = bfs::status(lock_path, ec);
      if(ec != boost::system::error_code()) {
         if(rc.type() == bfs::file_not_found) {
            appbase::app().generate_interrupt_request();
            FC_ASSERT( false, "Lock file removed while keosd still running.  Terminating.");
         }
      }
      t->expires_from_now(boost::posix_time::seconds(1));
      start_lock_watch(t);
   });
}

void beekeeper_wallet_manager::initialize_lock() {
   //This is technically somewhat racy in here -- if multiple keosd are in this function at once.
   //I've considered that an acceptable tradeoff to maintain cross-platform boost constructs here
   lock_path = dir / "wallet.lock";
   {
      std::ofstream x(lock_path.string());
      FC_ASSERT( !x.fail(), "Failed to open wallet lock file at ${f}", ("f", lock_path.string()));
   }
   wallet_dir_lock = std::make_unique<boost::interprocess::file_lock>(lock_path.string().c_str());
   if(!wallet_dir_lock->try_lock()) {
      wallet_dir_lock.reset();
      FC_ASSERT( false, "Failed to lock access to wallet directory; is another keosd running?");
   }
   auto timer = std::make_shared<boost::asio::deadline_timer>(appbase::app().get_io_service(), boost::posix_time::seconds(1));
   start_lock_watch(timer);
}

info beekeeper_wallet_manager::get_info()
{
   return time.get_info();
}

} //beekeeper

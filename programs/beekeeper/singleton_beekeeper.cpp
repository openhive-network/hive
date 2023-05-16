#include <beekeeper/singleton_beekeeper.hpp>

#include <appbase/application.hpp>

#include <boost/filesystem.hpp>

namespace beekeeper {
  namespace bfs = boost::filesystem;

  singleton_beekeeper::singleton_beekeeper( const boost::filesystem::path& _wallet_directory ): wallet_directory( _wallet_directory )
  {
    pid_file        = wallet_directory / "beekeeper.pid";
    connection_file = wallet_directory / "beekeeper.connection";
    lock_path_file  = wallet_directory / "beekeeper.wallet.lock";
  }

  singleton_beekeeper::~singleton_beekeeper()
  {
    if( wallet_dir_lock )
        bfs::remove( lock_path_file );
  }

  void singleton_beekeeper::start_lock_watch( std::shared_ptr<boost::asio::deadline_timer> t )
  {
    t->async_wait([t, this](const boost::system::error_code& /*ec*/)
    {
        boost::system::error_code ec;
        auto rc = bfs::status( lock_path_file, ec );
        if( ec != boost::system::error_code() )
        {
          if( rc.type() == bfs::file_not_found )
          {
              appbase::app().generate_interrupt_request();
              FC_ASSERT( false, "Lock file removed while keosd still running.  Terminating.");
          }
        }
        t->expires_from_now( boost::posix_time::seconds(1) );
        start_lock_watch(t);
    });
  }

  void singleton_beekeeper::initialize_lock()
  {
    //This is technically somewhat racy in here -- if multiple keosd are in this function at once.
    //I've considered that an acceptable tradeoff to maintain cross-platform boost constructs here
    {
        std::ofstream x( lock_path_file.string() );
        FC_ASSERT( !x.fail(), "Failed to open wallet lock file at ${f}", ("f", lock_path_file.string()));
    }

    wallet_dir_lock = std::make_unique<boost::interprocess::file_lock>( lock_path_file.string().c_str() );
    if( !wallet_dir_lock->try_lock() )
    {
        wallet_dir_lock.reset();
        FC_ASSERT( false, "Failed to lock access to wallet directory; is another keosd running?");
    }

    auto timer = std::make_shared<boost::asio::deadline_timer>( appbase::app().get_io_service(), boost::posix_time::seconds(1) );
    start_lock_watch(timer);
  }

  void singleton_beekeeper::save_pid()
  {

  }

  void singleton_beekeeper::save_connection_data()
  {

  }

  void singleton_beekeeper::start()
  {
    initialize_lock();
  }

  void singleton_beekeeper::close()
  {

  }

  boost::filesystem::path singleton_beekeeper::create_wallet_filename( const std::string& wallet_name ) const
  {
    return wallet_directory / ( wallet_name + file_ext );
  }

  bool singleton_beekeeper::started() const
  {
    return instance_started;
  }

  void singleton_beekeeper::save_data()
  {

  }

}
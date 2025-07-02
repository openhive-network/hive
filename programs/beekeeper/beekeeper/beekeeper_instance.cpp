#include <beekeeper/beekeeper_instance.hpp>

#include <appbase/application.hpp>

#include <fc/io/json.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/filesystem.hpp>

#include <fstream>

namespace beekeeper {
  namespace bfs = boost::filesystem;

  beekeeper_instance::beekeeper_instance( appbase::application& app, const boost::filesystem::path& wallet_directory )
                    : app( app )
  {
    lock_path_file  = wallet_directory / "beekeeper.wallet.lock";
  }

  beekeeper_instance::~beekeeper_instance()
  {
    if( is_instance_started() )
    {
      if( wallet_dir_lock )
          bfs::remove( lock_path_file );
    }
  }

  void beekeeper_instance::start_lock_watch( std::shared_ptr<boost::asio::deadline_timer> t )
  {
    t->async_wait([t, this](const boost::system::error_code& e)
    {
        if (e) return;
        boost::system::error_code ec;
        auto rc = bfs::status( lock_path_file, ec );
        if( ec != boost::system::error_code() )
        {
          if( rc.type() == bfs::file_not_found )
          {
              app.generate_interrupt_request();
              elog( "The lock file is removed while `beekeeper` is still running. Terminating." );
          }
        }
        t->expires_from_now( boost::posix_time::seconds(1) );
        start_lock_watch(t);
    });
  }

  void beekeeper_instance::initialize_lock()
  {
    {
        std::ofstream x( lock_path_file.string() );
        if( x.fail() )
        {
          wlog( "Failed to open wallet lock file at ${f}", ("f", lock_path_file.string()) );
          return;
        }
    }

    wallet_dir_lock = std::make_unique<boost::interprocess::file_lock>( lock_path_file.string().c_str() );
    if( !wallet_dir_lock->try_lock() )
    {
        wallet_dir_lock.reset();
        elog( "Failed to lock access to a wallet directory. Probably another beekeeper instance is running." );
        throw std::runtime_error( "A working directory is taken by another beekeeper instance" );
    }

    instance_started = true;

    auto timer = std::make_shared<boost::asio::deadline_timer>( app.get_io_service(), boost::posix_time::seconds(1) );
    start_lock_watch(timer);
  }

  bool beekeeper_instance::start()
  {
    initialize_lock();

    return is_instance_started();
  }

}

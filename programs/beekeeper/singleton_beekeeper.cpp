#include <beekeeper/singleton_beekeeper.hpp>

#include <appbase/application.hpp>

#include <fc/io/json.hpp>

#include <boost/exception/diagnostic_information.hpp>
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
    if( instance_started )
    {
      if( wallet_dir_lock )
          bfs::remove( lock_path_file );

      bfs::remove( pid_file );
      bfs::remove( connection_file );
    }
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
              FC_ASSERT( false, "Lock file removed while `beekeeper` still running. Terminating.");
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
        wlog( "Failed to lock access to wallet directory; is another `beekeeper` running?" );
        return;
    }

    instance_started = true;

    auto timer = std::make_shared<boost::asio::deadline_timer>( appbase::app().get_io_service(), boost::posix_time::seconds(1) );
    start_lock_watch(timer);
  }


  template<typename content_type>
  void singleton_beekeeper::write_to_file( const boost::filesystem::path& file_name, const content_type& content )
  {
    std::ofstream _file( file_name.string() );
    _file << content;
    _file.flush();
    _file.close();
  }

  fc::variant singleton_beekeeper::read_file( const boost::filesystem::path& file_name )
  {
    try
    {
      return fc::json::from_file( file_name.string() );
    }
    catch ( const boost::exception& e )
    {
      auto _message = boost::diagnostic_information(e);
      std::cerr << _message << "\n";
      return _message;
    }
    catch ( const fc::exception& e )
    {
      auto _message = e.to_detail_string();
      std::cerr << _message << "\n";
      return _message;
    }
    catch ( const std::exception& e )
    {
      auto _message = e.what();
      std::cerr << _message << "\n";
      return _message;
    }
    catch ( ... )
    {
      auto _message = "unknown exception\n";
      std::cerr << _message << "\n";
      return _message;
    }
  }

  void singleton_beekeeper::save_pid()
  {
    std::stringstream ss;
    ss << getpid();

    std::map<std::string, std::string> _map;
    _map["pid"] = ss.str();

    auto _json = fc::json::to_string( _map );
    write_to_file( pid_file, _json );
  }

  void singleton_beekeeper::send_fail_notification()
  {
    std::vector<fc::variant> _content;

    fc::variant _pid_v        = read_file( pid_file );
    fc::variant _connection_v = read_file( connection_file );

    _content.push_back( "Opening beekeeper failed" );
    _content.push_back( _pid_v );
    _content.push_back( _connection_v );

    auto _json = fc::json::to_string( _content );

    hive::notify_hived_status( _json );
  }

  bool singleton_beekeeper::start()
  {
    initialize_lock();

    if( instance_started )
      save_pid();
    else
      send_fail_notification();

    return instance_started;
  }

  boost::filesystem::path singleton_beekeeper::create_wallet_filename( const std::string& wallet_name ) const
  {
    return wallet_directory / ( wallet_name + file_ext );
  }

  void singleton_beekeeper::save_connection_details( const collector_t& values )
  {
    if( instance_started )
    {
      auto _json = fc::json::to_string( values );
      write_to_file( connection_file, _json );
    }
  }
}
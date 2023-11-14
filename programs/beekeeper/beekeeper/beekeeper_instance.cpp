#include <beekeeper/beekeeper_instance.hpp>

#include <appbase/application.hpp>

#include <fc/io/json.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/filesystem.hpp>

namespace beekeeper {
  namespace bfs = boost::filesystem;

  beekeeper_instance::beekeeper_instance( appbase::application& app, const boost::filesystem::path& wallet_directory, const std::string& notifications_endpoint )
                    : app( app ), error_notifications_endpoint( notifications_endpoint )
  {
    pid_file        = wallet_directory / "beekeeper.pid";
    connection_file = wallet_directory / "beekeeper.connection";
    lock_path_file  = wallet_directory / "beekeeper.wallet.lock";
  }

  beekeeper_instance::~beekeeper_instance()
  {
    if( instance_started )
    {
      if( wallet_dir_lock )
          bfs::remove( lock_path_file );

      bfs::remove( pid_file );
      bfs::remove( connection_file );
    }
  }

  void beekeeper_instance::start_lock_watch( std::shared_ptr<boost::asio::deadline_timer> t )
  {
    t->async_wait([t, this](const boost::system::error_code& /*ec*/)
    {
        boost::system::error_code ec;
        auto rc = bfs::status( lock_path_file, ec );
        if( ec != boost::system::error_code() )
        {
          if( rc.type() == bfs::file_not_found )
          {
              app.generate_interrupt_request();
              FC_ASSERT( false, "Lock file removed while `beekeeper` still running. Terminating.");
          }
        }
        t->expires_from_now( boost::posix_time::seconds(1) );
        start_lock_watch(t);
    });
  }

  void beekeeper_instance::initialize_lock()
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

    auto timer = std::make_shared<boost::asio::deadline_timer>( app.get_io_service(), boost::posix_time::seconds(1) );
    start_lock_watch(timer);
  }


  template<typename content_type>
  void beekeeper_instance::write_to_file( const boost::filesystem::path& file_name, const content_type& content )
  {
    std::ofstream _file( file_name.string() );
    _file << content;
    _file.flush();
    _file.close();
  }

  fc::variant beekeeper_instance::read_file( const boost::filesystem::path& file_name )
  {
    try
    {
      return fc::json::from_file( file_name.string() );
    }
    catch ( const boost::exception& e )
    {
      auto _message = boost::diagnostic_information(e);
      elog(_message);
      return _message;
    }
    catch ( const fc::exception& e )
    {
      auto _message = e.to_detail_string();
      elog(_message);
      return _message;
    }
    catch ( const std::exception& e )
    {
      auto _message = e.what();
      elog(_message);
      return _message;
    }
    catch ( ... )
    {
      auto _message = "unknown exception\n";
      elog(_message);
      return _message;
    }
  }

  void beekeeper_instance::save_pid()
  {
    std::stringstream ss;
    ss << getpid();

    std::map<std::string, std::string> _map;
    _map["pid"] = ss.str();

    auto _json = fc::json::to_string( _map );
    write_to_file( pid_file, _json );
  }

  void beekeeper_instance::send_fail_notification()
  {
    fc::variant _pid        = read_file( pid_file );
    fc::variant _connection = read_file( connection_file );

    auto _map_pid = _pid.as<std::map<std::string, std::string>>();
    auto __pid = _map_pid["pid"];

    hive::utilities::notifications::notification_handler_wrapper _notification_handler;
    _notification_handler.register_endpoint( error_notifications_endpoint );

    appbase::application::dynamic_notify( _notification_handler, "Opening beekeeper failed", "pid", __pid, "connection", _connection );
  }

  bool beekeeper_instance::start()
  {
    initialize_lock();

    if( instance_started )
      save_pid();
    else
      send_fail_notification();

    return instance_started;
  }

  void beekeeper_instance::save_connection_details( const collector_t& values )
  {
    if( instance_started )
    {
      auto _json = fc::json::to_string( values.value );
      write_to_file( connection_file, _json );
    }
  }
}
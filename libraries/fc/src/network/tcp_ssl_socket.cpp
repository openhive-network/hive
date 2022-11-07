#include <fc/network/tcp_socket.hpp>
#include <fc/network/ip.hpp>
#include <fc/network/tcp_socket_io_hooks.hpp>
#include <fc/network/have_so_reuseport.hpp>
#include <fc/fwd_impl.hpp>
#include <fc/asio.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/stdio.hpp>
#include <fc/exception/exception.hpp>

#include <boost/asio/ssl.hpp>

#if defined _WIN32 || defined WIN32 || defined OS_WIN64 || defined _WIN64 || defined WIN64 || defined WINNT
# include <mstcpip.h>
#endif

namespace fc {

  class tcp_ssl_socket::impl : public tcp_ssl_socket_io_hooks {
    public:
      impl() :
        ssl_context(boost::asio::ssl::context::tlsv12_client),
        _sock(fc::asio::default_io_service(), ssl_context),
        _io_hooks(this)
      {
        ssl_context.set_default_verify_paths();
        ssl_context.set_options(boost::asio::ssl::context::default_workarounds |
                      boost::asio::ssl::context::no_sslv2 |
                      boost::asio::ssl::context::no_sslv3 |
                      boost::asio::ssl::context::no_tlsv1 |
                      boost::asio::ssl::context::no_tlsv1_1 |
                      boost::asio::ssl::context::single_dh_use);
        _sock.set_verify_mode( boost::asio::ssl::verify_peer );
      }
      ~impl()
      {
        if( _sock.next_layer().is_open() )
          try
          {
            _sock.next_layer().close();
          }
          catch( ... )
          {}
        if( _read_in_progress.valid() )
          try
          {
            _read_in_progress.wait();
          }
          catch ( ... )
          {
          }
        if( _write_in_progress.valid() )
          try
          {
            _write_in_progress.wait();
          }
          catch ( ... )
          {
          }
      }
      virtual size_t readsome(ssl_socket& socket, char* buffer, size_t length) override;
      virtual size_t readsome(ssl_socket& socket, const std::shared_ptr<char>& buffer, size_t length, size_t offset) override;
      virtual size_t writesome(ssl_socket& socket, const char* buffer, size_t length) override;
      virtual size_t writesome(ssl_socket& socket, const std::shared_ptr<const char>& buffer, size_t length, size_t offset) override;

      fc::future<size_t> _write_in_progress;
      fc::future<size_t> _read_in_progress;
      boost::asio::ssl::context ssl_context;
      ssl_socket _sock;
      tcp_ssl_socket_io_hooks* _io_hooks;
  };

  size_t tcp_ssl_socket::impl::readsome(ssl_socket& socket, char* buffer, size_t length)
  {
    return (_read_in_progress = fc::asio::read_some(socket, buffer, length)).wait();
  }
  size_t tcp_ssl_socket::impl::readsome(ssl_socket& socket, const std::shared_ptr<char>& buffer, size_t length, size_t offset)
  {
    return (_read_in_progress = fc::asio::read_some(socket, buffer, length, offset)).wait();
  }
  size_t tcp_ssl_socket::impl::writesome(ssl_socket& socket, const char* buffer, size_t length)
  {
    return (_write_in_progress = fc::asio::write_some(socket, buffer, length)).wait();
  }
  size_t tcp_ssl_socket::impl::writesome(ssl_socket& socket, const std::shared_ptr<const char>& buffer, size_t length, size_t offset)
  {
    return (_write_in_progress = fc::asio::write_some(socket, buffer, length, offset)).wait();
  }


  tcp_ssl_socket::tcp_ssl_socket()
  : my( std::make_shared< impl >() ) {};

  tcp_ssl_socket::~tcp_ssl_socket(){};

  void tcp_ssl_socket::set_verify_peer( bool verify )
  {
    my->_sock.set_verify_mode( verify ? boost::asio::ssl::verify_peer : boost::asio::ssl::verify_none );
  }

  void tcp_ssl_socket::open()
  {
    my->_sock.next_layer().open(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0).protocol());
  }

  bool tcp_ssl_socket::is_open()const {
    return my->_sock.next_layer().is_open();
  }

  void tcp_ssl_socket::flush() {}
  void tcp_ssl_socket::close() {
    try {
        if( is_open() )
        {
          my->_sock.next_layer().close();
        }
    } FC_RETHROW_EXCEPTIONS( warn, "error closing tcp socket" );
  }

  bool tcp_ssl_socket::eof()const {
    return !my->_sock.next_layer().is_open();
  }

  size_t tcp_ssl_socket::writesome(const char* buf, size_t len)
  {
    return my->_io_hooks->writesome(my->_sock, buf, len);
  }

  size_t tcp_ssl_socket::writesome(const std::shared_ptr<const char>& buf, size_t len, size_t offset)
  {
    return my->_io_hooks->writesome(my->_sock, buf, len, offset);
  }

  fc::ip::endpoint tcp_ssl_socket::remote_endpoint()const
  {
    try
    {
      auto rep = my->_sock.next_layer().remote_endpoint();
      return  fc::ip::endpoint(rep.address().to_v4().to_ulong(), rep.port() );
    }
    FC_RETHROW_EXCEPTIONS( warn, "error getting socket's remote endpoint" );
  }


  fc::ip::endpoint tcp_ssl_socket::local_endpoint() const
  {
    try
    {
      auto boost_local_endpoint = my->_sock.next_layer().local_endpoint();
      return fc::ip::endpoint(boost_local_endpoint.address().to_v4().to_ulong(), boost_local_endpoint.port() );
    }
    FC_RETHROW_EXCEPTIONS( warn, "error getting socket's local endpoint" );
  }

  size_t tcp_ssl_socket::readsome( char* buf, size_t len )
  {
    return my->_io_hooks->readsome(my->_sock, buf, len);
  }

  size_t tcp_ssl_socket::readsome( const std::shared_ptr<char>& buf, size_t len, size_t offset ) {
    return my->_io_hooks->readsome(my->_sock, buf, len, offset);
  }

  void tcp_ssl_socket::connect_to( const fc::ip::endpoint& remote_endpoint, const std::string& hostname ) {
    fc::asio::tcp::connect(my->_sock.next_layer(), fc::asio::tcp::endpoint( boost::asio::ip::address_v4(remote_endpoint.get_address()), remote_endpoint.port() ) );
    my->_sock.set_verify_callback(boost::asio::ssl::rfc2818_verification(hostname));
    my->_sock.set_verify_depth(10);
		if (!SSL_set_tlsext_host_name(my->_sock.native_handle(), hostname.c_str()))
		{
			boost::system::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
			throw boost::system::system_error{ ec };
		}
    my->_sock.handshake(boost::asio::ssl::stream_base::client);
  }

  void tcp_ssl_socket::bind(const fc::ip::endpoint& local_endpoint)
  {
    try
    {
      my->_sock.next_layer().bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4(local_endpoint.get_address()),
                                                                                local_endpoint.port()));
    }
    catch (const std::exception& except)
    {
      elog("Exception binding outgoing connection to desired local endpoint ${endpoint}: ${what}", ("endpoint", local_endpoint)("what", except.what()));
      FC_THROW("error binding to ${endpoint}: ${what}", ("endpoint", local_endpoint)("what", except.what()));
    }
  }

  void tcp_ssl_socket::enable_keep_alives(const fc::microseconds& interval)
  {
    if (interval.count())
    {
      boost::asio::socket_base::keep_alive option(true);
      my->_sock.next_layer().set_option(option);
#if defined _WIN32 || defined WIN32 || defined OS_WIN64 || defined _WIN64 || defined WIN64 || defined WINNT
      struct tcp_keepalive keepalive_settings;
      keepalive_settings.onoff = 1;
      keepalive_settings.keepalivetime = (ULONG)(interval.count() / fc::milliseconds(1).count());
      keepalive_settings.keepaliveinterval = (ULONG)(interval.count() / fc::milliseconds(1).count());

      DWORD dwBytesRet = 0;
      if (WSAIoctl(my->_sock.native(), SIO_KEEPALIVE_VALS, &keepalive_settings, sizeof(keepalive_settings),
                   NULL, 0, &dwBytesRet, NULL, NULL) == SOCKET_ERROR)
        wlog("Error setting TCP keepalive values");
#elif !defined(__clang__) || (__clang_major__ >= 6)
      // This should work for modern Linuxes and for OSX >= Mountain Lion
      int timeout_sec = interval.count() / fc::seconds(1).count();
      if (setsockopt(my->_sock.next_layer().native_handle(), IPPROTO_TCP,
      #if defined( __APPLE__ )
                     TCP_KEEPALIVE,
       #else
                     TCP_KEEPIDLE,
       #endif
                     (char*)&timeout_sec, sizeof(timeout_sec)) < 0)
        wlog("Error setting TCP keepalive idle time");
# if !defined(__APPLE__) || defined(TCP_KEEPINTVL) // TCP_KEEPINTVL not defined before 10.9
      if (setsockopt(my->_sock.next_layer().native_handle(), IPPROTO_TCP, TCP_KEEPINTVL,
                     (char*)&timeout_sec, sizeof(timeout_sec)) < 0)
        wlog("Error setting TCP keepalive interval");
# endif // !__APPLE__ || TCP_KEEPINTVL
#endif // !WIN32
    }
    else
    {
      boost::asio::socket_base::keep_alive option(false);
      my->_sock.next_layer().set_option(option);
    }
  }

  void tcp_ssl_socket::set_io_hooks(tcp_ssl_socket_io_hooks* new_hooks)
  {
    my->_io_hooks = new_hooks ? new_hooks : &*my;
  }

  void tcp_ssl_socket::set_reuse_address(bool enable /* = true */)
  {
    FC_ASSERT(my->_sock.next_layer().is_open());
    boost::asio::socket_base::reuse_address option(enable);
    my->_sock.next_layer().set_option(option);
#if defined(__APPLE__) || defined(__linux__)
# ifndef SO_REUSEPORT
#  define SO_REUSEPORT 15
# endif
    // OSX needs SO_REUSEPORT in addition to SO_REUSEADDR.
    // This probably needs to be set for any BSD
    if (fc::detail::have_so_reuseport)
    {
      int reuseport_value = 1;
      if (setsockopt(my->_sock.next_layer().native_handle(), SOL_SOCKET, SO_REUSEPORT,
                     (char*)&reuseport_value, sizeof(reuseport_value)) < 0)
      {
        if (errno == ENOPROTOOPT)
          fc::detail::have_so_reuseport = false;
        else
          wlog("Error setting SO_REUSEPORT");
      }
    }
#endif // __APPLE__
  }

  int tcp_ssl_socket::set_receive_buffer_size(int new_receive_buffer_size)
  {
    //read and log old receive_buffer_size
    boost::asio::socket_base::receive_buffer_size old_receive_buffer_reading;
    my->_sock.next_layer().get_option(old_receive_buffer_reading);
    ddump((old_receive_buffer_reading.value()));

    boost::asio::socket_base::receive_buffer_size option(new_receive_buffer_size);
    my->_sock.next_layer().set_option(option);

    //read new value and log receive_buffer_size
    boost::asio::socket_base::receive_buffer_size new_receive_buffer_reading;
    my->_sock.next_layer().get_option(new_receive_buffer_reading);
    ddump((new_receive_buffer_reading.value()));
    return new_receive_buffer_reading.value();
  }

  int tcp_ssl_socket::set_send_buffer_size(int new_send_buffer_size)
  {
    //read and log old send_buffer_size
    boost::asio::socket_base::send_buffer_size old_send_buffer_reading;
    my->_sock.next_layer().get_option(old_send_buffer_reading);
    ddump((old_send_buffer_reading.value()));

    boost::asio::socket_base::send_buffer_size option(new_send_buffer_size);
    my->_sock.next_layer().set_option(option);

    //read new value and log send_buffer_size
    boost::asio::socket_base::send_buffer_size new_send_buffer_reading;
    my->_sock.next_layer().get_option(new_send_buffer_reading);
    ddump((new_send_buffer_reading.value()));
    return new_send_buffer_reading.value();
  }

  bool tcp_ssl_socket::get_no_delay()
  {
    boost::asio::ip::tcp::no_delay option;
    my->_sock.next_layer().get_option(option);
    return option.value();
  }

  void tcp_ssl_socket::set_no_delay(bool no_delay_flag)
  {
    boost::asio::ip::tcp::no_delay option(no_delay_flag);
    my->_sock.next_layer().set_option(option);
  }

} // namespace fc

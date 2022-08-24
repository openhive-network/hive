#pragma once
#include <fc/utility.hpp>
#include <fc/fwd.hpp>
#include <fc/io/iostream.hpp>
#include <fc/time.hpp>

#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace fc {
  namespace ip { class endpoint; }

  class tcp_socket_io_hooks;
  class tcp_ssl_socket_io_hooks;

  namespace detail
  {
#if defined(__APPLE__) || defined(__linux__)
    static bool have_so_reuseport = true;
#endif
  }

  class tcp_socket : public virtual iostream
  {
    public:
      tcp_socket();
      virtual ~tcp_socket();

      void     connect_to( const fc::ip::endpoint& remote_endpoint );
      void     bind( const fc::ip::endpoint& local_endpoint );
      void     enable_keep_alives(const fc::microseconds& interval);
      void     set_io_hooks(tcp_socket_io_hooks* new_hooks);
      void     set_reuse_address(bool enable = true); // set SO_REUSEADDR
      int      set_receive_buffer_size(int new_receive_buffer_size);
      int      set_send_buffer_size(int new_send_buffer_size);
      bool     get_no_delay();
      void     set_no_delay(bool no_delay_flag);
      fc::ip::endpoint remote_endpoint() const;
      fc::ip::endpoint local_endpoint() const;

      using istream::get;
      void get( char& c ) { read( &c, 1 ); }

      /// istream interface
      /// @{
      virtual size_t   readsome( char* buffer, size_t max );
      virtual size_t   readsome(const std::shared_ptr<char>& buffer, size_t max, size_t offset);
      virtual bool     eof()const;
      /// @}

      /// ostream interface
      /// @{
      virtual size_t   writesome( const char* buffer, size_t len );
      virtual size_t   writesome(const std::shared_ptr<const char>& buffer, size_t len, size_t offset);
      virtual void     flush();
      virtual void     close();
      /// @}

      void open();
      bool   is_open()const;

    private:
      friend class tcp_server;
      class impl;
      #if BOOST_VERSION >= 107000
      fc::fwd<impl,0x70> my;
      #else
      fc::fwd<impl,0x54> my;
      #endif
  };
  typedef std::shared_ptr<tcp_socket> tcp_socket_ptr;

  class tcp_ssl_socket : public virtual iostream
  {
    public:
      tcp_ssl_socket();
      virtual ~tcp_ssl_socket();

      void     connect_to( const fc::ip::endpoint& remote_endpoint );
      void     bind( const fc::ip::endpoint& local_endpoint );
      void     enable_keep_alives(const fc::microseconds& interval);
      void     set_io_hooks(tcp_ssl_socket_io_hooks* new_hooks);
      void     set_reuse_address(bool enable = true); // set SO_REUSEADDR
      int      set_receive_buffer_size(int new_receive_buffer_size);
      int      set_send_buffer_size(int new_send_buffer_size);
      bool     get_no_delay();
      void     set_no_delay(bool no_delay_flag);
      fc::ip::endpoint remote_endpoint() const;
      fc::ip::endpoint local_endpoint() const;

      using istream::get;
      void get( char& c ) { read( &c, 1 ); }

      /// istream interface
      /// @{
      virtual size_t   readsome( char* buffer, size_t max );
      virtual size_t   readsome(const std::shared_ptr<char>& buffer, size_t max, size_t offset);
      virtual bool     eof()const;
      /// @}

      /// ostream interface
      /// @{
      virtual size_t   writesome( const char* buffer, size_t len );
      virtual size_t   writesome(const std::shared_ptr<const char>& buffer, size_t len, size_t offset);
      virtual void     flush();
      virtual void     close();
      /// @}

      void open();
      bool   is_open()const;

    private:
      friend class tcp_server;
      class impl;
      #if BOOST_VERSION >= 107000
      fc::fwd<impl,0x178> my;
      #else
      fc::fwd<impl,0x15C> my;
      #endif
  };
  typedef std::shared_ptr<tcp_ssl_socket> tcp_ssl_socket_ptr;


  class tcp_server
  {
    public:
      tcp_server();
      ~tcp_server();

      void     close();
      void     accept( tcp_socket& s );
      void     set_reuse_address(bool enable = true); // set SO_REUSEADDR, call before listen
      int      set_receive_buffer_size(int new_receive_buffer_size);
      int      set_send_buffer_size(int new_send_buffer_size);
      bool     get_no_delay();
      void     set_no_delay(bool no_delay_flag);
      void     listen( uint16_t port );
      void     listen( const fc::ip::endpoint& ep );
      fc::ip::endpoint get_local_endpoint() const;
      uint16_t get_port()const;
    private:
      // non copyable
      tcp_server( const tcp_server& );
      tcp_server& operator=(const tcp_server& s );

      class impl;
      impl* my;
  };

} // namesapce fc


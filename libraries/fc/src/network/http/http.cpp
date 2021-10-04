#include <fc/network/http/http.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/io/sstream.hpp>
#include <fc/network/ip.hpp>
#include <fc/io/stdio.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/url.hpp>
#include <fc/asio.hpp>

#include <boost/system/error_code.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/rfc2818_verification.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_service.hpp>

#include <string>
#include <mutex>
#include <memory>

namespace fc { namespace http {

  namespace detail {
    /// A handle to uniquely identify a connection
    typedef std::weak_ptr< void >                                  connection_hdl;

    /// Called once for every successful http connection attempt
    typedef std::function< void( connection_hdl& ) >               open_handler;

    /// Called once for every successfully established connection after it is no longer capable of sending or receiving new messages
    typedef std::function< void( connection_hdl& ) >               close_handler;

    /// Called once for every unsuccessful WebSocket connection attempt
    typedef std::function< void( connection_hdl& ) >               fail_handler;

    // Message payload type
    typedef std::string                                            message_type;

    /// Called after a new message has been received
    typedef std::function< void( connection_hdl&, message_type ) > message_handler;

    typedef boost::asio::ssl::context                              ssl_context;
    typedef std::shared_ptr< ssl_context >                         ssl_context_ptr;

    /// Called when needed to request a TLS context for the library to use
    typedef std::function< ssl_context_ptr( connection_hdl& ) >    tls_init_handler;

    typedef boost::asio::io_service*                               io_service_ptr;

    /// acceptor being used
    typedef boost::asio::ip::tcp::acceptor                         acceptor_type;
    typedef std::shared_ptr< acceptor_type >                       acceptor_ptr;

    typedef boost::asio::io_service::strand                        strand_type;
    /// Type of a pointer to the Asio io_service::strand being used
    typedef std::shared_ptr< strand_type >                         strand_ptr;

    enum class connection_state
    {
      uninitialized = 0,
      ready         = 1,
      reading       = 2
    };

    enum class endpoint_state
    {
      uninitialized = 0,
      ready         = 1,
      listening     = 2
    };

    enum class session_state
    {
      connecting = 0,
      open       = 1,
      closing    = 2,
      closed     = 3
    };

    struct config
    {
      static constexpr bool enable_multithreading = true;

      /// Maximum size of close frame reason
      static uint8_t const close_reason_size = 123;
    };

    class connection_base
    {
    public:
      virtual ~connection_base() {}

      virtual constexpr bool is_secure()const = 0;
      virtual void init_asio ( io_service_ptr service ) = 0;

      /// Close the connection
      void close( uint16_t code, const message_type& reason );

      void send(const message_type& payload );

      /// Set Connection Handle
      void set_handle( connection_hdl hdl )
      {
        m_hdl = hdl;
      }

    protected:
      /// Current connection state
      session_state       m_session_state;
      endpoint_state      m_endpoint_state;
      connection_state    m_connection_state;
      strand_ptr          m_strand;

      /// Handlers mutex
      std::mutex          m_handlers_mutex;
      std::mutex          m_connection_state_lock;

      connection_hdl      m_hdl;
      io_service_ptr      m_io_service;
      acceptor_ptr        m_acceptor;
    };

    namespace tls {
      class connection : public connection_base
      {
      public:
        typedef boost::asio::ssl::stream< boost::asio::ip::tcp::socket > socket_type;
        typedef std::shared_ptr< socket_type >                           socket_ptr;

        /// called after the socket object is created but before it is used
        typedef std::function< void( connection_hdl&, socket_type& ) >
            socket_init_handler;

        virtual ~connection() {}

        virtual constexpr bool is_secure()const { return true; }

        // Handlers //
        void set_tls_init_handler( tls_init_handler&& _handler )
        {
          std::lock_guard< std::mutex > _guard( m_handlers_mutex );
          m_tls_init_handler = _handler;
        }

        /// Retrieve a pointer to the wrapped socket
        socket_type& get_socket() {
            return *m_socket;
        }

        /// initialize asio transport with external io_service
        virtual void init_asio ( io_service_ptr service ) override
        {
          FC_ASSERT( connection_base::m_connection_state == connection_state::uninitialized, "Invalid state" );

          FC_ASSERT( m_tls_init_handler, "Missing tls init handler" );

          m_io_service = service;

          m_acceptor = std::make_shared< acceptor_type >(*service);

          if ( config::enable_multithreading )
            m_strand = std::make_shared< strand_type >( *service );

          m_context = m_tls_init_handler(m_hdl);
          FC_ASSERT( m_context, "Invalid tls context" );

          m_socket = std::make_shared< socket_type >(*service, *m_context);

          if ( m_socket_init_handler )
            m_socket_init_handler(m_hdl, get_socket());

          if ( m_socket_init_handler )
            m_socket_init_handler( m_hdl, *m_socket );

          m_connection_state = connection_state::ready;
        }

      protected:
        // Handlers
        socket_init_handler m_socket_init_handler;
        tls_init_handler    m_tls_init_handler;

        socket_ptr          m_socket;
        ssl_context_ptr     m_context;
      };
    } // tls

    namespace unsecure {
      class connection : public connection_base
      {
      public:
        typedef boost::asio::ip::tcp::socket   socket_type;
        typedef std::shared_ptr< socket_type > socket_ptr;

        /// called after the socket object is created but before it is used
        typedef std::function< void( connection_hdl&, socket_type& ) >
            socket_init_handler;

        virtual ~connection() {}

        virtual constexpr bool is_secure()const { return false; }

        /// Retrieve a pointer to the wrapped socket
        socket_type& get_socket() {
            return *m_socket;
        }

        /// initialize asio transport with external io_service
        virtual void init_asio ( io_service_ptr service ) override
        {
          FC_ASSERT( connection_base::m_connection_state == connection_state::uninitialized, "Invalid state" );

          m_io_service = service;

          m_acceptor = std::make_shared< acceptor_type >(*service);

          if ( config::enable_multithreading )
            m_strand = std::make_shared< strand_type >(*service);

          m_socket = std::make_shared< socket_type >(*service);

          if ( m_socket_init_handler )
            m_socket_init_handler( m_hdl, *m_socket );

          m_connection_state = connection_state::ready;
        }

      protected:
        // Handlers
        socket_init_handler m_socket_init_handler;

        socket_ptr       m_socket;
      };
    } // unsecure

    template< typename ConnectionType >
    class endpoint : public ConnectionType
    {
    public:
      typedef ConnectionType                     connection_type;
      typedef std::shared_ptr< connection_type > connection_ptr;

      virtual ~endpoint() {}

      /// Retrieves a connection_ptr from a connection_hdl
      connection_ptr get_con_from_hdl( connection_hdl& hdl )
      {
        connection_ptr con = std::static_pointer_cast< connection_type >( hdl.lock() );
        FC_ASSERT( con, "Bad connection" );
        return con;
      }

      /// Sets whether to use the SO_REUSEADDR flag when opening listening sockets
      void set_reuse_addr( bool value );

      /// Check if the endpoint is listening
      bool is_listening()const
      {
        return (connection_type::m_endpoint_state == connection_type::endpoint_state::listening);
      }

      /// Stop listening
      void stop_listening()
      {
        FC_ASSERT( connection_type::m_endpoint_state == endpoint_state::listening, "asio::listen called from the wrong state" );

        connection_type::m_acceptor->close();
        connection_type::m_endpoint_state = endpoint_state::listening;
      }

      /// Set up endpoint for listening on a port
      void listen( uint16_t port );
      /// Set up endpoint for listening manually
      void listen( const boost::asio::ip::tcp::endpoint& ep );

      // Handlers //
      void set_open_handler( open_handler&& _handler )
      {
        std::lock_guard< std::mutex > _guard( connection_type::m_handlers_mutex );
        m_open_handler = _handler;
      }
      void set_message_handler( message_handler&& _handler )
      {
        std::lock_guard< std::mutex > _guard( connection_type::m_handlers_mutex );
        m_message_handler = _handler;
      }
      void set_close_handler( close_handler&& _handler )
      {
        std::lock_guard< std::mutex > _guard( connection_type::m_handlers_mutex );
        m_close_handler = _handler;
      }
      void set_fail_handler( fail_handler&& _handler )
      {
        std::lock_guard< std::mutex > _guard( connection_type::m_handlers_mutex );
        m_fail_handler = _handler;
      }

    protected:
      open_handler        m_open_handler;
      message_handler     m_message_handler;
      close_handler       m_close_handler;
      fail_handler        m_fail_handler;
    };

    template< typename ConnectionType >
    class client_endpoint : public endpoint< ConnectionType >
    {
    public:
      virtual ~client_endpoint() {}

      /// Create and initialize a new connection. You should then call connect() in order to perform a handshake
      void create_connection( const fc::url& _url, boost::system::error_code& ec );

      /// Initiates the opening connection handshake for connection
      void connect();

    protected:
    };

    template< typename ConnectionType >
    class server_endpoint : public endpoint< ConnectionType >
    {
    public:
      virtual ~server_endpoint() {}

      /// Starts the server's async connection acceptance loop
      void start_accept();

      /// Create and initialize a new connection
      void create_connection();

    protected:
    };

    template< typename ConnectionType >
    class http_connection_impl : public http_connection
    {
    public:
      typedef ConnectionType connection_type;

      http_connection_impl( connection_type con )
        : _http_connection( con )
      {}

      virtual ~http_connection_impl() {};

      virtual void send_message( const std::string& message )override
      {
        idump((message));
        _http_connection->send( message );
      }
      virtual void close( int64_t code, const std::string& reason )override
      {
        _http_connection->close( code,reason );
      }

      connection_type _http_connection;
    };

    class http_server_impl
    {
    public:
      http_server_impl()
      {}
    };

    class http_tls_server_impl
    {
    public:
      http_tls_server_impl( const std::string& server_pem, const std::string& ssl_password )
      {}
    };

    class http_client_impl
    {
    private:
      typedef std::shared_ptr< unsecure::connection >  con_impl_ptr;

    public:
      typedef client_endpoint< unsecure::connection >  endpoint_type;
      typedef std::shared_ptr< endpoint_type >         endpoint_ptr;
      typedef http_connection_impl< con_impl_ptr >     con_type;
      typedef std::shared_ptr< con_type >              con_ptr;

      http_client_impl()
        : _client_thread( fc::thread::current() )
      {
        _client.set_message_handler( [&]( connection_hdl hdl, message_type msg ){
          _client_thread.async( [&](){
            wdump((msg));
            fc::async( [=](){ if( _connection ) _connection->on_message(msg); });
          }).wait();
        });
        _client.set_close_handler( [=]( connection_hdl hdl ){
          _client_thread.async( [&](){ if( _connection ) {_connection->closed(); _connection.reset();} } ).wait();
          if( _closed ) _closed->set_value();
        });
        _client.set_fail_handler( [=]( connection_hdl hdl ){
          auto con = _client.get_con_from_hdl(hdl);
          if( _connection )
            _client_thread.async( [&](){ if( _connection ) _connection->closed(); _connection.reset(); } ).wait();
          if( _closed )
              _closed->set_value();
        });
        _client.init_asio( &fc::asio::default_io_service() );
      }
      ~http_client_impl()
      {
        if( _connection )
        {
          _connection->close( 0, "client closed" );
          _connection.reset();
          _closed->wait();
        }
      }

      fc::promise<void>::ptr  _connected;
      fc::promise<void>::ptr  _closed;
      endpoint_type           _client;
      con_ptr                 _connection;
      fc::url                 _url;

    private:
      fc::thread&             _client_thread;
    };

    class http_tls_client_impl
    {
    private:
      typedef std::shared_ptr< tls::connection >       con_impl_ptr;

    public:
      typedef client_endpoint< tls::connection >       endpoint_type;
      typedef std::shared_ptr< endpoint_type >         endpoint_ptr;
      typedef http_connection_impl< con_impl_ptr >     con_type;
      typedef std::shared_ptr< con_type >              con_ptr;

      http_tls_client_impl( const std::string& ca_filename )
        : _client_thread( fc::thread::current() )
      {
        _client.set_message_handler( [&]( connection_hdl hdl, message_type msg ){
          _client_thread.async( [&](){
            wdump((msg));
            fc::async( [=](){ if( _connection ) _connection->on_message(msg); });
          }).wait();
        });
        _client.set_close_handler( [=]( connection_hdl hdl ){
          _client_thread.async( [&](){ if( _connection ) {_connection->closed(); _connection.reset();} } ).wait();
          if( _closed ) _closed->set_value();
        });
        _client.set_fail_handler( [=]( connection_hdl hdl ){
          auto con = _client.get_con_from_hdl(hdl);
          if( _connection )
            _client_thread.async( [&](){ if( _connection ) _connection->closed(); _connection.reset(); } ).wait();
          if( _closed )
              _closed->set_value();
        });

        _client.set_tls_init_handler( [=]( connection_hdl ) {
          ssl_context_ptr ctx = std::make_shared< boost::asio::ssl::context >( boost::asio::ssl::context::tlsv1 );
          try {
            ctx->set_options(boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::single_dh_use);

            setup_peer_verify( ctx, ca_filename );
          } FC_CAPTURE_AND_LOG(());
          return ctx;
        });

        _client.init_asio( &fc::asio::default_io_service() );
      }
      ~http_tls_client_impl()
      {
        if( _connection )
        {
          _connection->close( 0, "client closed" );
          _connection.reset();
          _closed->wait();
        }
      }

      void setup_peer_verify( ssl_context_ptr& ctx, const std::string& ca_filename )
      {
        if( ca_filename == "_none" )
          return;
        ctx->set_verify_mode( boost::asio::ssl::verify_peer );
        if( ca_filename == "_default" )
          ctx->set_default_verify_paths();
        else
          ctx->load_verify_file( ca_filename );
        ctx->set_verify_depth(10);
        FC_ASSERT( _url.host().valid(), "Host not in given url: ${url}", ("url",_url) );
        ctx->set_verify_callback( boost::asio::ssl::rfc2818_verification( *_url.host() ) );
      }

      fc::promise<void>::ptr  _connected;
      fc::promise<void>::ptr  _closed;
      endpoint_type           _client;
      con_ptr                 _connection;
      fc::url                 _url;

    private:
      fc::thread&             _client_thread;
    };
  } // namespace detail

  http_server::http_server()
      : server(), my( new detail::http_server_impl() ) {}
  http_server::~http_server() {}

  void http_server::on_connection( const on_connection_handler& handler )
  {}
  void http_server::listen( uint16_t port )
  {}
  void http_server::listen( const fc::ip::endpoint& ep )
  {}
  void http_server::start_accept()
  {}


  http_tls_server::http_tls_server( const std::string& server_pem, const std::string& ssl_password )
    : server( server_pem, ssl_password ), my( new detail::http_tls_server_impl(server::server_pem, server::ssl_password) ) {}
  http_tls_server::~http_tls_server() {}

  void http_tls_server::on_connection( const on_connection_handler& handler )
  {}
  void http_tls_server::listen( uint16_t port )
  {}
  void http_tls_server::listen( const fc::ip::endpoint& ep )
  {}
  void http_tls_server::start_accept()
  {}


  http_client::http_client()
    : client(), my( new detail::http_client_impl() ) {}
  http_client::~http_client() {}

  connection_ptr http_client::connect( const std::string& _url_str )
  { try {
    fc::url _url{ _url_str };
    FC_ASSERT( _url.proto() == "http", "Invalid protocol: \"{proto}\". Expected: \"http\"", ("proto", _url.proto()) );
    my->_url = _url;

    my->_connected = fc::promise<void>::ptr( new fc::promise<void>("http::connect") );

    my->_client.set_open_handler( [=]( detail::connection_hdl hdl ){
      my->_connection = std::make_shared< detail::http_client_impl::con_type >( my->_client.get_con_from_hdl( hdl ) );
      my->_closed = fc::promise<void>::ptr( new fc::promise<void>("http::closed") );
      my->_connected->set_value();
    });

    my->_connected->wait();
    my->_client.connect();
    return my->_connection;
  } FC_CAPTURE_AND_RETHROW( (_url_str) )}


  http_tls_client::http_tls_client( const std::string& ca_filename )
    : client( ca_filename ), my( new detail::http_tls_client_impl( client::ca_filename ) ) {}
  http_tls_client::~http_tls_client() {}

  connection_ptr http_tls_client::connect( const std::string& _url_str )
  { try {
    fc::url _url{ _url_str };
    FC_ASSERT( _url.proto() == "https", "Invalid protocol: \"{proto}\". Expected: \"https\"", ("proto", _url.proto()) );
    my->_url = _url;

    my->_connected = fc::promise<void>::ptr( new fc::promise<void>("https::connect") );

    my->_client.set_open_handler( [=]( detail::connection_hdl hdl ){
      my->_connection = std::make_shared< detail::http_tls_client_impl::con_type >( my->_client.get_con_from_hdl( hdl ) );
      my->_closed = fc::promise<void>::ptr( new fc::promise<void>("https::closed") );
      my->_connected->set_value();
    });

    my->_connected->wait();
    return nullptr;
  } FC_CAPTURE_AND_RETHROW( (_url_str) ) }

} } // fc::http

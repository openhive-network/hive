#include <fc/network/tcp_socket.hpp>
#include <fc/network/ip.hpp>
#include <fc/network/tcp_socket_io_hooks.hpp>
#include <fc/network/have_so_reuseport.hpp>
#include <fc/fwd_impl.hpp>
#include <fc/asio.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/stdio.hpp>
#include <fc/exception/exception.hpp>

#if defined _WIN32 || defined WIN32 || defined OS_WIN64 || defined _WIN64 || defined WIN64 || defined WINNT
# include <mstcpip.h>
#endif

namespace fc {

class tcp_socket::impl : public tcp_socket_io_hooks {
public:
    impl() :
        _sock(fc::asio::default_io_service()),
        _io_hooks(this)
    {}
    ~impl()
    {
        if( _sock.is_open() )
            try
            {
                _sock.close();
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
    virtual size_t readsome(boost::asio::ip::tcp::socket& socket, char* buffer, size_t length) override;
    virtual size_t readsome(boost::asio::ip::tcp::socket& socket, const std::shared_ptr<char>& buffer, size_t length, size_t offset) override;
    virtual size_t writesome(boost::asio::ip::tcp::socket& socket, const char* buffer, size_t length) override;
    virtual size_t writesome(boost::asio::ip::tcp::socket& socket, const std::shared_ptr<const char>& buffer, size_t length, size_t offset) override;

    fc::future<size_t> _write_in_progress;
    fc::future<size_t> _read_in_progress;
    boost::asio::ip::tcp::socket _sock;
    tcp_socket_io_hooks* _io_hooks;
};

size_t tcp_socket::impl::readsome(boost::asio::ip::tcp::socket& socket, char* buffer, size_t length)
{
    return (_read_in_progress = fc::asio::read_some(socket, buffer, length)).wait();
}
size_t tcp_socket::impl::readsome(boost::asio::ip::tcp::socket& socket, const std::shared_ptr<char>& buffer, size_t length, size_t offset)
{
    return (_read_in_progress = fc::asio::read_some(socket, buffer, length, offset)).wait();
}
size_t tcp_socket::impl::writesome(boost::asio::ip::tcp::socket& socket, const char* buffer, size_t length)
{
    return (_write_in_progress = fc::asio::write_some(socket, buffer, length)).wait();
}
size_t tcp_socket::impl::writesome(boost::asio::ip::tcp::socket& socket, const std::shared_ptr<const char>& buffer, size_t length, size_t offset)
{
    return (_write_in_progress = fc::asio::write_some(socket, buffer, length, offset)).wait();
}

void tcp_socket::open()
{
#ifdef ENABLE_IPV6
    boost::asio::ip::v6_only option(false);
    my->_sock.set_option(option);
    my->_sock.open(boost::asio::ip::tcp::v6());
#else
    my->_sock.open(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0).protocol());
#endif
}
#ifdef ENABLE_IPV6
void tcp_socket::open(const fc::ip::endpoint& ep)
{
    if (ep.get_address().is_v4() && false)
        my->_sock.open(boost::asio::ip::tcp::v4());
    else {
        my->_sock.open(boost::asio::ip::tcp::v6());
        // allow dual-stack unless you want v6-only
        boost::asio::ip::v6_only option(false);
        my->_sock.set_option(option);
    }
}
#endif

bool tcp_socket::is_open()const {
    return my->_sock.is_open();
}

tcp_socket::tcp_socket()
    : my( std::make_shared< impl >() ) {}

tcp_socket::~tcp_socket(){};

void tcp_socket::flush() {}
void tcp_socket::close() {
    try {
        if( is_open() )
        {
            my->_sock.close();
        }
    } FC_RETHROW_EXCEPTIONS( warn, "error closing tcp socket" );
}

bool tcp_socket::eof()const {
    return !my->_sock.is_open();
}

size_t tcp_socket::writesome(const char* buf, size_t len)
{
    return my->_io_hooks->writesome(my->_sock, buf, len);
}

size_t tcp_socket::writesome(const std::shared_ptr<const char>& buf, size_t len, size_t offset)
{
    return my->_io_hooks->writesome(my->_sock, buf, len, offset);
}

fc::ip::endpoint tcp_socket::remote_endpoint() const
{
    try
    {
        auto rep = my->_sock.remote_endpoint();
        return  fc::ip::endpoint(rep.address().to_v4().to_ulong(), rep.port() );
    }
    FC_RETHROW_EXCEPTIONS( warn, "error getting socket's remote endpoint" );
    try
    {
        auto rep = my->_sock.remote_endpoint();
        const auto& addr = rep.address();

#ifndef ENABLE_IPV6
        return fc::ip::endpoint(addr.to_v4().to_ulong(), rep.port());
#else
        fc::ip::address fcaddr;
        if (addr.is_v4() && false)
        {
            fcaddr = fc::ip::address(addr.to_v4().to_ulong());
        }
        else
        {
            auto bytes = addr.to_v6().to_bytes();
            fcaddr = fc::ip::address(bytes);
        }
        return fc::ip::endpoint(fcaddr, rep.port());
#endif
    }
    FC_RETHROW_EXCEPTIONS(warn, "error getting socket's remote endpoint");
}


fc::ip::endpoint tcp_socket::local_endpoint() const
{
    try
    {
        auto boost_local_endpoint = my->_sock.local_endpoint();
        return fc::ip::endpoint(boost_local_endpoint.address().to_v4().to_ulong(), boost_local_endpoint.port() );
    }
    FC_RETHROW_EXCEPTIONS( warn, "error getting socket's local endpoint" );
}

size_t tcp_socket::readsome( char* buf, size_t len )
{
    return my->_io_hooks->readsome(my->_sock, buf, len);
}

size_t tcp_socket::readsome( const std::shared_ptr<char>& buf, size_t len, size_t offset ) {
    return my->_io_hooks->readsome(my->_sock, buf, len, offset);
}

void tcp_socket::connect_to( const fc::ip::endpoint& remote_endpoint ) {
    fc::asio::tcp::connect(my->_sock, fc::asio::tcp::endpoint( boost::asio::ip::address_v4(remote_endpoint.get_address()), remote_endpoint.port() ) );
}

void tcp_socket::bind(const fc::ip::endpoint& local_endpoint)
#ifndef ENABLE_IPV6
{
    try
    {
        my->_sock.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4(local_endpoint.get_address()),
                                                      local_endpoint.port()));
    }
    catch (const std::exception& except)
    {
        elog("Exception binding outgoing connection to desired local endpoint ${endpoint}: ${what}", ("endpoint", local_endpoint)("what", except.what()));
        FC_THROW("error binding to ${endpoint}: ${what}", ("endpoint", local_endpoint)("what", except.what()));
    }
}
#else
{
  try
  {
    boost::asio::ip::address addr;
    fc::ip::address _address = local_endpoint.get_address();
    std::array<uint8_t,16> ip = _address.raw();
    if (_address.is_v4())
    {
      std::array<unsigned char, 4> v4bytes = {
        ip[12],
        ip[13],
        ip[14],
        ip[15]
      };
    addr = boost::asio::ip::address_v4(v4bytes);
    }
    else
    {
      addr = boost::asio::ip::address_v6(ip);
      my->_sock.set_option(boost::asio::ip::v6_only(false));
    }

    my->_sock.bind(
      boost::asio::ip::tcp::endpoint(
        addr,
        local_endpoint.port()
        )
      );
  }
  catch (const std::exception& except)
  {
    elog("Exception binding outgoing connection to desired local endpoint ${endpoint}: ${what}",
        ("endpoint", local_endpoint)("what", except.what()));
    FC_THROW("error binding to ${endpoint}: ${what}",
            ("endpoint", local_endpoint)("what", except.what()));
  }
}
#endif

void tcp_socket::enable_keep_alives(const fc::microseconds& interval)
{
    if (interval.count())
    {
        boost::asio::socket_base::keep_alive option(true);
        my->_sock.set_option(option);
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
        if (setsockopt(my->_sock.native_handle(), IPPROTO_TCP,
#if defined( __APPLE__ )
                       TCP_KEEPALIVE,
#else
                       TCP_KEEPIDLE,
#endif
                       (char*)&timeout_sec, sizeof(timeout_sec)) < 0)
            wlog("Error setting TCP keepalive idle time");
#if !defined(__APPLE__) || defined(TCP_KEEPINTVL) // TCP_KEEPINTVL not defined before 10.9
        if (setsockopt(my->_sock.native_handle(), IPPROTO_TCP, TCP_KEEPINTVL,
                       (char*)&timeout_sec, sizeof(timeout_sec)) < 0)
            wlog("Error setting TCP keepalive interval");
# endif // !__APPLE__ || TCP_KEEPINTVL
#endif // !WIN32
    }
    else
    {
        boost::asio::socket_base::keep_alive option(false);
        my->_sock.set_option(option);
    }
}

void tcp_socket::set_io_hooks(tcp_socket_io_hooks* new_hooks)
{
    my->_io_hooks = new_hooks ? new_hooks : &*my;
}

void tcp_socket::set_reuse_address(bool enable /* = true */)
{
    FC_ASSERT(my->_sock.is_open());
    boost::asio::socket_base::reuse_address option(enable);
    my->_sock.set_option(option);
#if defined(__APPLE__) || defined(__linux__)
# ifndef SO_REUSEPORT
#  define SO_REUSEPORT 15
# endif
    // OSX needs SO_REUSEPORT in addition to SO_REUSEADDR.
    // This probably needs to be set for any BSD
    if (fc::detail::have_so_reuseport)
    {
        int reuseport_value = 1;
        if (setsockopt(my->_sock.native_handle(), SOL_SOCKET, SO_REUSEPORT,
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

int tcp_socket::set_receive_buffer_size(int new_receive_buffer_size)
{
    //read and log old receive_buffer_size
    boost::asio::socket_base::receive_buffer_size old_receive_buffer_reading;
    my->_sock.get_option(old_receive_buffer_reading);
    ddump((old_receive_buffer_reading.value()));

    boost::asio::socket_base::receive_buffer_size option(new_receive_buffer_size);
    my->_sock.set_option(option);

    //read new value and log receive_buffer_size
    boost::asio::socket_base::receive_buffer_size new_receive_buffer_reading;
    my->_sock.get_option(new_receive_buffer_reading);
    ddump((new_receive_buffer_reading.value()));
    return new_receive_buffer_reading.value();
}

int tcp_socket::set_send_buffer_size(int new_send_buffer_size)
{
    //read and log old send_buffer_size
    boost::asio::socket_base::send_buffer_size old_send_buffer_reading;
    my->_sock.get_option(old_send_buffer_reading);
    ddump((old_send_buffer_reading.value()));

    boost::asio::socket_base::send_buffer_size option(new_send_buffer_size);
    my->_sock.set_option(option);

    //read new value and log send_buffer_size
    boost::asio::socket_base::send_buffer_size new_send_buffer_reading;
    my->_sock.get_option(new_send_buffer_reading);
    ddump((new_send_buffer_reading.value()));
    return new_send_buffer_reading.value();
}

bool tcp_socket::get_no_delay()
{
    boost::asio::ip::tcp::no_delay option;
    my->_sock.get_option(option);
    return option.value();
}

void tcp_socket::set_no_delay(bool no_delay_flag)
{
    boost::asio::ip::tcp::no_delay option(no_delay_flag);
    my->_sock.set_option(option);
}

class tcp_server::impl {
public:
    impl()
        :_accept( fc::asio::default_io_service() )
    {
        _accept.open(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0).protocol());
    }

    ~impl(){
        try {
            _accept.close();
        }
        catch ( boost::system::system_error& )
        {
            wlog( "unexpected exception ${e}", ("e", fc::except_str()) );
        }
    }

    boost::asio::ip::tcp::acceptor _accept;
};
void tcp_server::close() {
    if( my && my->_accept.is_open() )
        my->_accept.close();
    my.reset();
}
tcp_server::tcp_server(){}
tcp_server::~tcp_server() {}


void tcp_server::accept( tcp_socket& s )
{
    try
    {
        FC_ASSERT( my.get() );
        fc::asio::tcp::accept( my->_accept, s.my->_sock  );
    } FC_RETHROW_EXCEPTIONS( warn, "Unable to accept connection on socket." );
}
void tcp_server::set_reuse_address(bool enable /* = true */)
{
    if( !my )
        my = std::make_shared< impl >();
    boost::asio::ip::tcp::acceptor::reuse_address option(enable);
    my->_accept.set_option(option);
#if defined(__APPLE__) || (defined(__linux__) && defined(SO_REUSEPORT))
    // OSX needs SO_REUSEPORT in addition to SO_REUSEADDR.
    // This probably needs to be set for any BSD
    if (fc::detail::have_so_reuseport)
    {
        int reuseport_value = 1;
        if (setsockopt(my->_accept.native_handle(), SOL_SOCKET, SO_REUSEPORT,
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

int tcp_server::set_receive_buffer_size(int new_receive_buffer_size)
{
    if( !my )
        my = std::make_shared< impl >();
    //read and log old receive_buffer_size
    boost::asio::socket_base::receive_buffer_size old_receive_buffer_reading;
    my->_accept.get_option(old_receive_buffer_reading);
    ddump((old_receive_buffer_reading.value()));

    boost::asio::socket_base::receive_buffer_size option(new_receive_buffer_size);
    my->_accept.set_option(option);

    //read and log new receive_buffer_size
    boost::asio::socket_base::receive_buffer_size new_receive_buffer_reading;
    my->_accept.get_option(new_receive_buffer_reading);
    ddump((new_receive_buffer_reading.value()));

    return new_receive_buffer_reading.value();
}

int tcp_server::set_send_buffer_size(int new_send_buffer_size)
{
    if( !my )
        my = std::make_shared< impl >();
    //read and log old send_buffer_size
    boost::asio::socket_base::send_buffer_size old_send_buffer_reading;
    my->_accept.get_option(old_send_buffer_reading);
    ddump((old_send_buffer_reading.value()));

    boost::asio::socket_base::send_buffer_size option(new_send_buffer_size);
    my->_accept.set_option(option);

    //read and log new send_buffer_size
    boost::asio::socket_base::send_buffer_size new_send_buffer_reading;
    my->_accept.get_option(new_send_buffer_reading);
    ddump((new_send_buffer_reading.value()));

    return new_send_buffer_reading.value();
}

bool tcp_server::get_no_delay()
{
    if (!my)
        my = std::make_shared< impl >();
    boost::asio::ip::tcp::no_delay option;
    my->_accept.get_option(option);
    return option.value();
}

void tcp_server::set_no_delay(bool no_delay_flag)
{
    if (!my)
        my = std::make_shared< impl >();
    boost::asio::ip::tcp::no_delay option(no_delay_flag);
    my->_accept.set_option(option);
}

void tcp_server::listen( uint16_t port )
#ifndef ENABLE_IPV6
{
    if( !my )
        my = std::make_shared< impl >();
    try
    {
        my->_accept.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4(), port));
        my->_accept.listen(256);
    }
    FC_RETHROW_EXCEPTIONS(warn, "error listening on socket");
}
#else
{
  if (!my)
    my = std::make_shared<impl>();
  try
  {
    fc::ip::address addr("[::]");
    fc::ip::endpoint ep(addr, port);
    listen(ep);
  }
  FC_RETHROW_EXCEPTIONS(warn, "error listening on socket");
}
#endif
void tcp_server::listen( const fc::ip::endpoint& ep )
#ifndef ENABLE_IPV6
{
    if( !my )
        my = std::make_shared< impl >();
    try
    {
        my->_accept.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::from_string((string)ep.get_address()), ep.port()));
        my->_accept.listen();
    }
    FC_RETHROW_EXCEPTIONS(warn, "error listening on socket");
}
#else
{
    if (!my)
        my = std::make_shared<impl>();
    try
    {
        fc::ip::address fcaddr = ep.get_address();
        std::string addr_str   = fc::string(fcaddr);

        boost::asio::ip::address addr;
        std::array<uint8_t, 16> _ip = ep.get_address().raw();
        bool isAny = std::all_of(_ip.begin(), _ip.end(), [](uint8_t b) {
                return b == 0;
            });
        if (isAny) {
            addr = boost::asio::ip::address_v6::any();
        } else if (ep.get_address().is_v4()) {
            addr = boost::asio::ip::address_v4(ep.get_address());
        } else {
            addr = boost::asio::ip::address_v6(ep.get_address().raw());
        }
        boost::asio::ip::tcp::endpoint endpoint(addr, ep.port());
        if (my->_accept.is_open())
        {
            boost::system::error_code ec;
            my->_accept.close(ec);
            ilog("Closed existing acceptor: ${err}", ("err", ec.message()));
        }
        ilog("Before Open to ${addr}", ("addr", addr_str));
        boost::system::error_code err;
        my->_accept.open(endpoint.protocol(),err);
        ilog("After Open: ${err}",("err",err.message()));
        if (!fcaddr.is_v4())
        {
            my->_accept.set_option(boost::asio::ip::v6_only(false));
        }
        my->_accept.set_option(boost::asio::socket_base::reuse_address(true),err);
        ilog("After set option to reuse address: ${err}",("err",err.message()));
        ilog("before Bind to ${addr}",("addr",endpoint.address().to_string()));
        my->_accept.bind(endpoint);
        ilog("Now binding ${addr}, port ${port}",
             ("addr",endpoint.address().to_string())
             ("port",endpoint.port()));
        my->_accept.listen(256);
    }
    FC_RETHROW_EXCEPTIONS(warn, "error listening on socket");
}
#endif

fc::ip::endpoint tcp_server::get_local_endpoint() const
{
    FC_ASSERT( my != nullptr );
    return fc::ip::endpoint(my->_accept.local_endpoint().address().to_string(),
                            my->_accept.local_endpoint().port() );
}

uint16_t tcp_server::get_port()const
{
    FC_ASSERT( my != nullptr );
    return my->_accept.local_endpoint().port();
}



} // namespace fc

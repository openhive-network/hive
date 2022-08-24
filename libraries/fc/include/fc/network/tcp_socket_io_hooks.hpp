#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>

namespace fc
{
  class tcp_socket_io_hooks
  {
  public:
    virtual size_t readsome(boost::asio::ip::tcp::socket& socket, char* buffer, size_t length) = 0;
    virtual size_t readsome(boost::asio::ip::tcp::socket& socket, const std::shared_ptr<char>& buffer, size_t length, size_t offset) = 0;
    virtual size_t writesome(boost::asio::ip::tcp::socket& socket, const char* buffer, size_t length) = 0;
    virtual size_t writesome(boost::asio::ip::tcp::socket& socket, const std::shared_ptr<const char>& buffer, size_t length, size_t offset) = 0;
  };
  class tcp_ssl_socket_io_hooks
  {
  protected:
    using socket = boost::asio::ip::tcp::socket;
    using ssl_socket = boost::asio::ssl::stream<socket>;

  public:
    virtual size_t readsome(ssl_socket& socket, char* buffer, size_t length) = 0;
    virtual size_t readsome(ssl_socket& socket, const std::shared_ptr<char>& buffer, size_t length, size_t offset) = 0;
    virtual size_t writesome(ssl_socket& socket, const char* buffer, size_t length) = 0;
    virtual size_t writesome(ssl_socket& socket, const std::shared_ptr<const char>& buffer, size_t length, size_t offset) = 0;
  };
} // namesapce fc

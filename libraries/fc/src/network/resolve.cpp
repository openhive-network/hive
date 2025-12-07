#include <fc/asio.hpp>
#include <fc/network/ip.hpp>
#include <fc/log/logger.hpp>

namespace fc
{

namespace {
  fc::ip::address from_asio_address(const boost::asio::ip::address& addr) {
    if (addr.is_v4()) {
      return fc::ip::address(ip::ipv4_address(addr.to_v4().to_ulong()));
    } else {
      auto bytes = addr.to_v6().to_bytes();
      std::array<uint8_t, 16> arr;
      std::copy(bytes.begin(), bytes.end(), arr.begin());
      return fc::ip::address(ip::ipv6_address(arr));
    }
  }

  fc::ip::endpoint from_asio_endpoint(const boost::asio::ip::tcp::endpoint& ep) {
    return fc::ip::endpoint(from_asio_address(ep.address()), ep.port());
  }
}

  std::vector<fc::ip::endpoint> resolve( const fc::string& host, uint16_t port )
  {
    auto ep = fc::asio::tcp::resolve( host, std::to_string(uint64_t(port)) );
    std::vector<fc::ip::endpoint> eps;
    eps.reserve(ep.size());
    // Prefer IPv6 addresses - add them first
    for( auto itr = ep.begin(); itr != ep.end(); ++itr )
    {
      if( itr->address().is_v6() )
      {
        eps.push_back( from_asio_endpoint(*itr) );
      }
    }
    // Then add IPv4 addresses
    for( auto itr = ep.begin(); itr != ep.end(); ++itr )
    {
      if( itr->address().is_v4() )
      {
        eps.push_back( from_asio_endpoint(*itr) );
      }
    }
    return eps;
  }

  std::vector< fc::ip::endpoint > resolve_string_to_ip_endpoints( const string& endpoint_string )
  {
    try
    {
      string hostname;
      string port_string;

      // Check for IPv6 bracketed notation: [IPv6]:port or [hostname]:port
      if (!endpoint_string.empty() && endpoint_string[0] == '[') {
        auto close_bracket = endpoint_string.find(']');
        FC_ASSERT(close_bracket != string::npos, "Missing closing bracket in endpoint string");

        hostname = endpoint_string.substr(1, close_bracket - 1);

        // Check for port after bracket
        if (close_bracket + 1 < endpoint_string.size()) {
          FC_ASSERT(endpoint_string[close_bracket + 1] == ':',
                    "Expected ':' after closing bracket in endpoint string");
          port_string = endpoint_string.substr(close_bracket + 2);
        } else {
          FC_THROW("Missing required port number in endpoint string \"${endpoint_string}\"",
                   ("endpoint_string", endpoint_string));
        }
      } else {
        // IPv4 format: hostname:port
        string::size_type colon_pos = endpoint_string.rfind(':');
        if (colon_pos == std::string::npos)
          FC_THROW("Missing required port number in endpoint string \"${endpoint_string}\"",
                   ("endpoint_string", endpoint_string));

        hostname = endpoint_string.substr(0, colon_pos);
        port_string = endpoint_string.substr(colon_pos + 1);
      }

      try
      {
        uint16_t port = boost::lexical_cast< uint16_t >( port_string );
        std::vector< fc::ip::endpoint > endpoints = resolve( hostname, port );

        if( endpoints.empty() )
          FC_THROW_EXCEPTION( unknown_host_exception, "The host name can not be resolved: ${hostname}", ("hostname", hostname) );

        return endpoints;
      }
      catch( const boost::bad_lexical_cast& )
      {
        FC_THROW("Bad port: ${port}", ("port", port_string) );
      }
    }
    FC_CAPTURE_AND_RETHROW( (endpoint_string) )
  }
}

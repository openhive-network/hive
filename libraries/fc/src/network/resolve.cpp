#include <fc/asio.hpp>
#include <fc/network/ip.hpp>
#include <fc/log/logger.hpp>
#ifdef ENABLE_IPV6
#include <boost/asio.hpp>
#endif
namespace fc
{
  std::vector<fc::ip::endpoint> resolve( const fc::string& host, uint16_t port )
  {
#ifdef ENABLE_IPV6
    using tcp = boost::asio::ip::tcp;
    auto ep = fc::asio::tcp::resolve(host, std::to_string(port));
#else
    auto ep = fc::asio::tcp::resolve( host, std::to_string(uint64_t(port)) );
#endif
    std::vector<fc::ip::endpoint> eps;
    eps.reserve(ep.size());
#ifdef ENABLE_IPV6
    for (const auto& r : ep)
    {
      auto addr = r.address();
      if (addr.is_v4())
      {
        eps.push_back(fc::ip::endpoint(addr.to_v4().to_ulong(), r.port()));
      }
      else if (addr.is_v6())
      {
        fc::ip::endpoint e;
        e.set_address_v6(addr.to_v6().to_bytes());
        e.set_port(r.port());
        eps.push_back(e);
      }
    }
#else
    for( auto itr = ep.begin(); itr != ep.end(); ++itr )
    {
      if( itr->address().is_v4() )
      {
       eps.push_back( fc::ip::endpoint(itr->address().to_v4().to_ulong(), itr->port()) );
      }
    }
#endif
    return eps;
  }
  std::vector< fc::ip::endpoint > resolve_string_to_ip_endpoints( const string& endpoint_string )
  {
#ifdef ENABLE_IPV6
    try
    {
      std::string host;
      std::string port_string;
      if (!endpoint_string.empty() && endpoint_string[0] == '[')
      {
        auto end = endpoint_string.find(']');
        FC_ASSERT(end != std::string::npos,"Malformed IPv6 endpoint: ${endpoint_string}", ("endpoint_string", endpoint_string));
        host = endpoint_string.substr(1, end - 1);

        FC_ASSERT(end + 2 < endpoint_string.size(),"Missing port in IPv6 endpoint: ${endpoint_string}", ("endpoint_string", endpoint_string));
        port_string = endpoint_string.substr(end + 2);
      }
      else
      {
        auto pos = endpoint_string.rfind(':');
        FC_ASSERT(pos != std::string::npos,"Missing port  in endpoint: ${endpoint_string}", ("endpoint_string", endpoint_string));
        host = endpoint_string.substr(0, pos);
        port_string = endpoint_string.substr(pos + 1);
      }
      uint16_t port;
      try
      {
        port = boost::lexical_cast<uint16_t>(port_string);
      }
      catch ( const boost::bad_lexical_cast& )
      {
        FC_THROW("Bad port: ${port}", ("port", port_string));
      }
      auto endpoints = resolve(host, port);
      FC_ASSERT(!endpoints.empty(),"Unable to resolve host: ${host}", ("host", host));
      return endpoints;
#else
      string::size_type colon_pos = endpoint_string.find( ':' );
      if( colon_pos == std::string::npos )
        FC_THROW( "Missing required port number in endpoint string \"${endpoint_string}\"",
                  ("endpoint_string", endpoint_string) );
        string port_string = endpoint_string.substr( colon_pos + 1 );
      try
      {
        uint16_t port = boost::lexical_cast< uint16_t >( port_string );
        string hostname = endpoint_string.substr( 0, colon_pos );
        std::vector< fc::ip::endpoint > endpoints = resolve( hostname, port );

        if( endpoints.empty() )
          FC_THROW_EXCEPTION( unknown_host_exception, "The host name can not be resolved: ${hostname}", ("hostname", hostname) );

        return endpoints;
      }
      catch( const boost::bad_lexical_cast& )
      {
        FC_THROW("Bad port: ${port}", ("port", port_string) );
      }
#endif
    }
    FC_CAPTURE_AND_RETHROW( (endpoint_string) )
  }
}

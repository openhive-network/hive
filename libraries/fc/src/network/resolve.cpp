#include <fc/asio.hpp>
#include <fc/network/ip.hpp>
#include <fc/log/logger.hpp>

namespace fc
{
  std::vector<fc::ip::endpoint> resolve( const fc::string& host, uint16_t port )
  {
    auto ep = fc::asio::tcp::resolve( host, std::to_string(uint64_t(port)) );
    std::vector<fc::ip::endpoint> eps;
    eps.reserve(ep.size());
    for( auto itr = ep.begin(); itr != ep.end(); ++itr )
    {
      const auto& addr = itr->address();

#ifndef ENABLE_IPV6
      if( addr.is_v4() )
      {
        // IPv4-only FC legacy build
        eps.push_back(fc::ip::endpoint(addr.to_v4().to_ulong(), itr->port()));
      }
#else
      fc::ip::address a(addr.to_string());
      eps.push_back(fc::ip::endpoint(a, itr->port()));
#endif
    }
    return eps;
  }

  std::vector< fc::ip::endpoint > resolve_string_to_ip_endpoints( const string& endpoint_string )
  {
    try
    {
      string host;
      string port_string;
#ifdef ENABLE_IPV6
      if (!endpoint_string.empty() && endpoint_string[0] == '[')
      {
        size_t end = endpoint_string.find(']');
        if (end == string::npos)
          FC_THROW("Invalid IPv6 endpoint string: ${s}", ("s", endpoint_string));
        host = endpoint_string.substr(1, end - 1);
        if (endpoint_string.size() <= end + 2 || endpoint_string[end+1] != ':')
          FC_THROW("Invalid IPv6 endpoint string (missing port): ${s}", ("s", endpoint_string));
        port_string = endpoint_string.substr(end + 2);
        uint16_t port = 0;
        try
        {
          port = boost::lexical_cast<uint16_t>(port_string);
        }
        catch (const boost::bad_lexical_cast&)
        {
          FC_THROW("Bad port: ${port}", ("port", port_string));
        }
        auto endpoints = resolve(host, port);
        if (endpoints.empty())
          FC_THROW_EXCEPTION(unknown_host_exception,
                             "Host name could not be resolved: ${host}", ("host", host));
        return endpoints;
      }
      else
#endif
      {
        string::size_type colon_pos = endpoint_string.find( ':' );
        if( colon_pos == std::string::npos )
          FC_THROW( "Missing required port number in endpoint string \"${endpoint_string}\"",
                    ("endpoint_string", endpoint_string) );
        host = endpoint_string.substr(0, colon_pos);
        port_string = endpoint_string.substr(colon_pos + 1);
        uint16_t port = 0;
        try
        {
            port = boost::lexical_cast<uint16_t>(port_string);
        }
        catch (const boost::bad_lexical_cast&)
        {
            FC_THROW("Bad port: ${port}", ("port", port_string));
        }
        auto endpoints = resolve(host, port);
        if (endpoints.empty())
          FC_THROW_EXCEPTION(unknown_host_exception,
                             "Host name could not be resolved: ${host}", ("host", host));
        return endpoints;
      }
    }
    FC_CAPTURE_AND_RETHROW( (endpoint_string) );
  }
}

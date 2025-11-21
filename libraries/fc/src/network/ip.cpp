#include <fc/network/ip.hpp>
#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <string>

namespace fc { namespace ip {

  address::address( uint32_t ip )
#ifndef ENABLE_IPV6
  :_ip(ip){}
#else
  {
      _ip = { ip, 0, 0, 0 };
  }

  address::address(std::array<uint32_t, 4> ip )
  {
      _ip = ip;
  }
#endif
  address::address( const fc::string& s )
#ifndef ENABLE_IPV6
  {
      try
      {
          _ip = boost::asio::ip::address_v4::from_string(s.c_str()).to_ulong();
      }
      FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address ${address}", ("address", s))
  }
#else
  {
    try
    {
      boost::system::error_code ec;
      // First try IPv4
      auto v4 = boost::asio::ip::address_v4::from_string(s.c_str(), ec);
      if (!ec)
      {
        // IPv4 → store in _ip[0]
        _ip[0] = v4.to_ulong();
        _ip[1] = 0;
        _ip[2] = 0;
        _ip[3] = 0;
        return;
      }
      // Now try IPv6
      ec = boost::system::error_code{};
      auto v6 = boost::asio::ip::address_v6::from_string(s.c_str(), ec);
      if (!ec)
      {
        auto bytes = v6.to_bytes();    // std::array<uint8_t,16>
        // Fill uint32_t words in network byte order
        _ip[0] = (bytes[0]  << 24) | (bytes[1]  << 16) | (bytes[2]  << 8) | bytes[3];
        _ip[1] = (bytes[4]  << 24) | (bytes[5]  << 16) | (bytes[6]  << 8) | bytes[7];
        _ip[2] = (bytes[8]  << 24) | (bytes[9]  << 16) | (bytes[10] << 8) | bytes[11];
        _ip[3] = (bytes[12] << 24) | (bytes[13] << 16) | (bytes[14] << 8) | bytes[15];
        return;
      }
      // Both IPv4 and IPv6 failed
      FC_THROW_EXCEPTION(fc::parse_error_exception, "Error parsing IP address ${address}", ("address", s));
    }
    FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address ${address}", ("address", s))
  }
#endif

  bool operator==( const address& a, const address& b )
#ifndef ENABLE_IPV6
  {
    return uint32_t(a) == uint32_t(b);
  }
#else
  {
    return a.raw() == b.raw();
  }
#endif
  bool operator!=( const address& a, const address& b )
#ifndef ENABLE_IPV6
  {
    return uint32_t(a) != uint32_t(b);
  }
#else
  {
    return !(a == b);
  }
#endif

  address& address::operator=( const fc::string& s )
#ifndef ENABLE_IPV6
  {
    try
    {
      _ip = boost::asio::ip::address_v4::from_string(s.c_str()).to_ulong();
    }
    FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address ${address}", ("address", s))
    return *this;
  }
#else
  {
    try
    {
      boost::system::error_code ec;
      //
      // 1. Try IPv4
      //
      auto v4 = boost::asio::ip::address_v4::from_string(s.c_str(), ec);
      if (!ec)
      {
        // IPv4 stored in _ip[0], rest zero
        _ip[0] = v4.to_ulong();
        _ip[1] = _ip[2] = _ip[3] = 0;
        return *this;
      }
      //
      // 2. Try IPv6
      //
      ec = boost::system::error_code{};
      auto v6 = boost::asio::ip::address_v6::from_string(s.c_str(), ec);
      if (!ec)
      {
        auto bytes = v6.to_bytes();   // std::array<uint8_t,16>
        // Convert 16 bytes → 4× uint32_t using network byte order
        _ip[0] = (uint32_t(bytes[0])  << 24) | (uint32_t(bytes[1])  << 16) |
                 (uint32_t(bytes[2])  << 8)  |  uint32_t(bytes[3]);
        _ip[1] = (uint32_t(bytes[4])  << 24) | (uint32_t(bytes[5])  << 16) |
                 (uint32_t(bytes[6])  << 8)  |  uint32_t(bytes[7]);
        _ip[2] = (uint32_t(bytes[8])  << 24) | (uint32_t(bytes[9])  << 16) |
                 (uint32_t(bytes[10]) << 8)  |  uint32_t(bytes[11]);
        _ip[3] = (uint32_t(bytes[12]) << 24) | (uint32_t(bytes[13]) << 16) |
                 (uint32_t(bytes[14]) << 8)  |  uint32_t(bytes[15]);
         return *this;
      }

      //
      // If both failed → throw
      //
      FC_THROW_EXCEPTION(fc::parse_error_exception, "Error parsing IP address ${address}", ("address", s));
    }
    FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address ${address}", ("address", s))
  }
#endif


  address::operator fc::string()const
#ifndef ENABLE_IPV6
  {
    try
    {
      return boost::asio::ip::address_v4(_ip).to_string().c_str();
    }
    FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address to string")
  }
#else
  {
    if(_ip[1] == 0 && _ip[2] == 0 && _ip[3] == 0)
    {
      try
      {
        return boost::asio::ip::address_v4(_ip[0]).to_string().c_str();
      }
      FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address to string")
    }
    try
    {
      std::array<uint8_t,16> bytes;
      for (int i = 0; i < 4; i++) {
        bytes[i*4]   = (_ip[i] >> 24) & 0xFF;
        bytes[i*4+1] = (_ip[i] >> 16) & 0xFF;
        bytes[i*4+2] = (_ip[i] >> 8)  & 0xFF;
        bytes[i*4+3] =  _ip[i]        & 0xFF;
      }
      boost::asio::ip::address_v6 v6(bytes);
      return v6.to_string();
    }
    FC_RETHROW_EXCEPTIONS(error, "Error rendering IPv6 address to string")
  }
#endif

  address::operator uint32_t()const
#ifndef ENABLE_IPV6
  {
    return _ip;
  }
#else
  {
    if(_ip[1] == 0 && _ip[2] == 0 && _ip[3] == 0)
      return _ip[0];
    FC_THROW_EXCEPTION(fc::parse_error_exception, "Cannot convert IPv6 address to uint32");
  }
#endif
  endpoint::endpoint()
  :_port(0){  }
  endpoint::endpoint(const address& a, uint16_t p)
  :_port(p),_ip(a){}

  bool operator==( const endpoint& a, const endpoint& b ) {
    return a._port == b._port  && a._ip == b._ip;
  }
  bool operator!=( const endpoint& a, const endpoint& b ) {
    return a._port != b._port || a._ip != b._ip;
  }

  bool operator< ( const endpoint& a, const endpoint& b )
  {
     return  uint32_t(a.get_address()) < uint32_t(b.get_address()) ||
             (uint32_t(a.get_address()) == uint32_t(b.get_address()) &&
              uint32_t(a.port()) < uint32_t(b.port()));
  }

  uint16_t       endpoint::port()const    { return _port; }
  const address& endpoint::get_address()const { return _ip;   }

  endpoint endpoint::from_string( const string& endpoint_string )
  {
#ifndef ENABLE_IPV6
    try
    {
      endpoint ep;
      auto pos = endpoint_string.find(':');
      ep._ip   = boost::asio::ip::address_v4::from_string(endpoint_string.substr( 0, pos ) ).to_ulong();
      ep._port = boost::lexical_cast<uint16_t>( endpoint_string.substr( pos+1, endpoint_string.size() ) );
      return ep;
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting string to IP endpoint")
#else
    try
    {
      endpoint ep;
      // IPv6: "[addr]:port"
      if (!endpoint_string.empty() && endpoint_string[0] == '[')
      {
        auto end = endpoint_string.find(']');
        FC_ASSERT(end != string::npos, "Malformed IPv6 endpoint: ${endpoint_string}",
                 ("endpoint_string", endpoint_string));

        string ip_str = endpoint_string.substr(1, end - 1);

        FC_ASSERT(end + 2 <= endpoint_string.size(),
                 "Missing port in IPv6 endpoint: ${endpoint_string}",
                 ("endpoint_string", endpoint_string));

        string port_str = endpoint_string.substr(end + 2);

        ep._ip = fc::ip::address(ip_str);
        ep._port = boost::lexical_cast<uint16_t>(port_str);
        return ep;
      }

      // IPv4: "IP:port"
      auto pos = endpoint_string.rfind(':');
      FC_ASSERT(pos != string::npos,
                "Missing port in endpoint: ${endpoint_string}",
                ("endpoint_string", endpoint_string));

      string ip_str = endpoint_string.substr(0, pos);
      string port_str = endpoint_string.substr(pos + 1);

      ep._ip = fc::ip::address(ip_str);
      ep._port = boost::lexical_cast<uint16_t>(port_str);
      return ep;
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting string to IP endpoint")
#endif
  }

#ifdef ENABLE_IPV6
  bool address::is_v4() const {
    return _ip[1] == 0 && _ip[2] == 0 && _ip[3] == 0;
  }
#endif

  endpoint::operator string()const 
  {
#ifndef ENABLE_IPV6
    try
    {
      return string(_ip) + ':' + fc::string(boost::lexical_cast<std::string>(_port).c_str());
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting IP endpoint to string")
#else
    try
    {
      string ip_str = (string)_ip;
      if (!_ip.is_v4())
        return "[" + ip_str + "]:" + std::to_string(_port);
      return ip_str + ":" + std::to_string(_port);
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting IP endpoint to string")
#endif
  }

  /**
   *  @return true if the ip is in the following ranges:
   *
   *  10.0.0.0    to 10.255.255.255
   *  172.16.0.0  to 172.31.255.255
   *  192.168.0.0 to 192.168.255.255
   *  169.254.0.0 to 169.254.255.255
   *
   */
  bool address::is_private_address()const
  {
    static address min10_ip("10.0.0.0");
    static address max10_ip("10.255.255.255");
    static address min172_ip("172.16.0.0");
    static address max172_ip("172.31.255.255");
    static address min192_ip("192.168.0.0");
    static address max192_ip("192.168.255.255");
    static address min169_ip("169.254.0.0");
    static address max169_ip("169.254.255.255");
    if( _ip >= min10_ip._ip && _ip <= max10_ip._ip ) return true;
    if( _ip >= min172_ip._ip && _ip <= max172_ip._ip ) return true;
    if( _ip >= min192_ip._ip && _ip <= max192_ip._ip ) return true;
    if( _ip >= min169_ip._ip && _ip <= max169_ip._ip ) return true;
    return false;
  }

  /**
   *  224.0.0.0 to 239.255.255.255
   */
  bool address::is_multicast_address()const
  {
    static address min_ip("224.0.0.0");
    static address max_ip("239.255.255.255");
    return  _ip >= min_ip._ip  && _ip <= max_ip._ip;
  }

  /** !private & !multicast */
  bool address::is_public_address()const
  {
    return !( is_private_address() || is_multicast_address() );
  }

}  // namespace ip

  void to_variant( const ip::endpoint& var,  variant& vo )
  {
    vo = fc::string(var);
  }
  void from_variant( const variant& var,  ip::endpoint& vo )
  {
    vo = ip::endpoint::from_string(var.as_string());
  }

  void to_variant( const ip::address& var,  variant& vo )
  {
    vo = fc::string(var);
  }
  void from_variant( const variant& var,  ip::address& vo )
  {
    vo = ip::address(var.as_string());
  }

} 

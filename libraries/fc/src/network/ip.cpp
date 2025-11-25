#include <fc/network/ip.hpp>
#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <string>

namespace fc { namespace ip {

  address::address( uint32_t ipv4 )
#ifndef ENABLE_IPV6
  :_ip(ip){}
#else
  {
      _ip = {0};
      _ip[10] = 0xFF;
      _ip[11] = 0xFF;
      _ip[12] = (ipv4 >> 24) & 0xFF;
      _ip[13] = (ipv4 >> 16) & 0xFF;
      _ip[14] = (ipv4 >> 8) & 0xFF;
      _ip[15] = ipv4 & 0xFF;
  }

  address::address(std::array<uint8_t, 16> ip )
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
    auto v4 = boost::asio::ip::address_v4::from_string(s.c_str(), ec);
    if (!ec)
    {
      uint32_t ipv4 = v4.to_ulong();
      _ip.fill(0);
      _ip[10] = 0xFF;
      _ip[11] = 0xFF;
      _ip[12] = (ipv4 >> 24) & 0xFF;
      _ip[13] = (ipv4 >> 16) & 0xFF;
      _ip[14] = (ipv4 >> 8) & 0xFF;
      _ip[15] = ipv4 & 0xFF;
      return;
    }
    ec = {};
    auto v6 = boost::asio::ip::address_v6::from_string(s.c_str(), ec);
    if (!ec)
    {
      _ip = v6.to_bytes();
      return;
    }
    FC_THROW_EXCEPTION(fc::parse_error_exception,
                       "Error parsing IP address ${address}",
                       ("address", s));
  }
  FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address ${address}", ("address", s));
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
      auto v4 = boost::asio::ip::address_v4::from_string(s.c_str(), ec);
      if (!ec)
      {
          auto bytes = v4.to_bytes(); // 4 bytes
          std::fill(_ip.begin(), _ip.end(), 0);
          _ip[10] = 0xFF;
          _ip[11] = 0xFF;
          _ip[12] = bytes[0];
          _ip[13] = bytes[1];
          _ip[14] = bytes[2];
          _ip[15] = bytes[3];

          return *this;
      }
      ec = {};
      auto v6 = boost::asio::ip::address_v6::from_string(s.c_str(), ec);
      if (!ec)
      {
          auto bytes = v6.to_bytes(); // 16 bytes
          std::copy(bytes.begin(), bytes.end(), _ip.begin());
          return *this;
      }
      FC_THROW_EXCEPTION(fc::parse_error_exception,
                         "Error parsing IP address ${address}",
                         ("address", s));
  }
  FC_RETHROW_EXCEPTIONS(error,
                        "Error parsing IP address ${address}",
                        ("address", s));
}
#endif


address::operator fc::string() const
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
    try
    {
      if (this->is_v4())
      {
        uint32_t ipv4 =
            (uint32_t(_ip[12]) << 24) |
            (uint32_t(_ip[13]) << 16) |
            (uint32_t(_ip[14]) <<  8) |
             uint32_t(_ip[15]);
        return boost::asio::ip::address_v4(ipv4).to_string();
      }
      std::array<uint8_t,16> bytes = _ip;
      boost::asio::ip::address_v6 v6(bytes);
      return v6.to_string();
    }
    FC_RETHROW_EXCEPTIONS(error, "Error converting IP address to string");
  }
#endif

  address::operator uint32_t()const
#ifndef ENABLE_IPV6
  {
    return _ip;
  }
#else
  {
    if(this->is_v4())
    {
      uint32_t ipv4 =
                (uint32_t(_ip[12]) << 24) |
                (uint32_t(_ip[13]) << 16) |
                (uint32_t(_ip[14]) <<  8) |
                uint32_t(_ip[15]);
      return ipv4;
    }
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

  uint16_t endpoint::port()const
  {
      return _port;
  }
  const address& endpoint::get_address()const
  {
      return _ip;
  }

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
  bool address::is_v4() const
  {
      return _ip[10] == 0xFF &&
             _ip[11] == 0xFF &&
             std::all_of(_ip.begin(), _ip.begin()+10,
                         [](uint8_t b){ return b == 0; });
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

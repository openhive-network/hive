#include <fc/network/ip.hpp>
#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <string>

namespace fc { namespace ip {

  //////////////////////////////////////////////////////////////////////////////
  // address implementation
  //////////////////////////////////////////////////////////////////////////////

  address::address()
    : _addr(ipv4_address(0))
  {}

  address::address(uint32_t ip)
    : _addr(ipv4_address(ip))
  {}

  address::address(const fc::string& s)
  {
    try
    {
      // Try to parse as boost::asio::ip::address which handles both IPv4 and IPv6
      auto boost_addr = boost::asio::ip::make_address(s.c_str());
      if (boost_addr.is_v4()) {
        _addr = ipv4_address(boost_addr.to_v4().to_ulong());
      } else {
        auto bytes = boost_addr.to_v6().to_bytes();
        std::array<uint8_t, 16> arr;
        std::copy(bytes.begin(), bytes.end(), arr.begin());
        _addr = ipv6_address(arr);
      }
    }
    FC_RETHROW_EXCEPTIONS(error, "Error parsing IP address ${address}", ("address", s))
  }

  address::address(const ipv4_address& v4)
    : _addr(v4)
  {}

  address::address(const ipv6_address& v6)
    : _addr(v6)
  {}

  bool operator==(const address& a, const address& b) {
    if (a._addr.which() != b._addr.which())
      return false;
    if (a.is_ipv4())
      return a.get_ipv4() == b.get_ipv4();
    else
      return a.get_ipv6() == b.get_ipv6();
  }

  bool operator!=(const address& a, const address& b) {
    return !(a == b);
  }

  bool operator<(const address& a, const address& b) {
    // IPv4 addresses sort before IPv6 addresses
    if (a._addr.which() != b._addr.which())
      return a._addr.which() < b._addr.which();
    if (a.is_ipv4())
      return a.get_ipv4() < b.get_ipv4();
    else
      return a.get_ipv6() < b.get_ipv6();
  }

  address& address::operator=(const fc::string& s)
  {
    *this = address(s);
    return *this;
  }

  address::operator fc::string() const
  {
    try
    {
      if (is_ipv4()) {
        return boost::asio::ip::address_v4(get_ipv4().addr).to_string().c_str();
      } else {
        boost::asio::ip::address_v6::bytes_type bytes;
        const auto& v6 = get_ipv6();
        std::copy(v6.addr.begin(), v6.addr.end(), bytes.begin());
        return boost::asio::ip::address_v6(bytes).to_string().c_str();
      }
    }
    FC_RETHROW_EXCEPTIONS(error, "Error converting IP address to string")
  }

  address::operator uint32_t() const {
    FC_ASSERT(is_ipv4(), "Cannot convert IPv6 address to uint32_t");
    return get_ipv4().addr;
  }

  bool address::is_ipv4() const {
    return _addr.which() == 0;
  }

  bool address::is_ipv6() const {
    return _addr.which() == 1;
  }

  const ipv4_address& address::get_ipv4() const {
    FC_ASSERT(is_ipv4(), "Address is not IPv4");
    return _addr.get<ipv4_address>();
  }

  const ipv6_address& address::get_ipv6() const {
    FC_ASSERT(is_ipv6(), "Address is not IPv6");
    return _addr.get<ipv6_address>();
  }

  bool address::is_private_address() const
  {
    if (is_ipv4()) {
      uint32_t ip = get_ipv4().addr;
      // 10.0.0.0/8
      if ((ip & 0xFF000000) == 0x0A000000) return true;
      // 172.16.0.0/12
      if ((ip & 0xFFF00000) == 0xAC100000) return true;
      // 192.168.0.0/16
      if ((ip & 0xFFFF0000) == 0xC0A80000) return true;
      // 169.254.0.0/16 (link-local)
      if ((ip & 0xFFFF0000) == 0xA9FE0000) return true;
      return false;
    } else {
      const auto& v6 = get_ipv6();
      // fc00::/7 (Unique Local Addresses) - first byte is fc or fd
      if ((v6.addr[0] & 0xFE) == 0xFC) return true;
      // fe80::/10 (Link-local)
      if (v6.addr[0] == 0xFE && (v6.addr[1] & 0xC0) == 0x80) return true;
      return false;
    }
  }

  bool address::is_multicast_address() const
  {
    if (is_ipv4()) {
      uint32_t ip = get_ipv4().addr;
      // 224.0.0.0/4
      return (ip & 0xF0000000) == 0xE0000000;
    } else {
      const auto& v6 = get_ipv6();
      // ff00::/8
      return v6.addr[0] == 0xFF;
    }
  }

  bool address::is_loopback_address() const
  {
    if (is_ipv4()) {
      uint32_t ip = get_ipv4().addr;
      // 127.0.0.0/8
      return (ip & 0xFF000000) == 0x7F000000;
    } else {
      const auto& v6 = get_ipv6();
      // ::1
      for (int i = 0; i < 15; ++i)
        if (v6.addr[i] != 0) return false;
      return v6.addr[15] == 1;
    }
  }

  bool address::is_public_address() const
  {
    return !(is_private_address() || is_multicast_address() || is_loopback_address());
  }

  //////////////////////////////////////////////////////////////////////////////
  // endpoint implementation
  //////////////////////////////////////////////////////////////////////////////

  endpoint::endpoint()
    : _port(0), _ip()
  {}

  endpoint::endpoint(const address& a, uint16_t p)
    : _port(p), _ip(a)
  {}

  endpoint::endpoint(const fc::string& addr_str, uint16_t p)
    : _port(p), _ip(address(addr_str))
  {}

  bool operator==(const endpoint& a, const endpoint& b) {
    return a._port == b._port && a._ip == b._ip;
  }

  bool operator!=(const endpoint& a, const endpoint& b) {
    return !(a == b);
  }

  bool operator<(const endpoint& a, const endpoint& b)
  {
    if (a._ip < b._ip) return true;
    if (b._ip < a._ip) return false;
    return a._port < b._port;
  }

  uint16_t endpoint::port() const { return _port; }
  const address& endpoint::get_address() const { return _ip; }

  endpoint endpoint::from_string(const string& endpoint_string)
  {
    try
    {
      endpoint ep;

      // Check for IPv6 bracketed notation: [IPv6]:port
      if (!endpoint_string.empty() && endpoint_string[0] == '[') {
        auto close_bracket = endpoint_string.find(']');
        FC_ASSERT(close_bracket != string::npos, "Missing closing bracket in IPv6 endpoint");

        string ipv6_str = endpoint_string.substr(1, close_bracket - 1);
        ep._ip = address(ipv6_str);

        // Check for port after bracket
        if (close_bracket + 1 < endpoint_string.size()) {
          FC_ASSERT(endpoint_string[close_bracket + 1] == ':',
                    "Expected ':' after closing bracket in IPv6 endpoint");
          string port_str = endpoint_string.substr(close_bracket + 2);
          ep._port = boost::lexical_cast<uint16_t>(port_str);
        }
      } else {
        // IPv4 format: IP:port
        auto pos = endpoint_string.rfind(':');
        FC_ASSERT(pos != string::npos, "Expected ':' separator in endpoint string");

        string addr_str = endpoint_string.substr(0, pos);
        string port_str = endpoint_string.substr(pos + 1);

        ep._ip = address(addr_str);
        ep._port = boost::lexical_cast<uint16_t>(port_str);
      }

      return ep;
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting string '${s}' to IP endpoint", ("s", endpoint_string))
  }

  endpoint::operator string() const
  {
    try
    {
      if (_ip.is_ipv6()) {
        // IPv6: [address]:port
        return "[" + fc::string(_ip) + "]:" + fc::string(boost::lexical_cast<std::string>(_port).c_str());
      } else {
        // IPv4: address:port
        return fc::string(_ip) + ':' + fc::string(boost::lexical_cast<std::string>(_port).c_str());
      }
    }
    FC_RETHROW_EXCEPTIONS(warn, "error converting IP endpoint to string")
  }

} // namespace ip

  //////////////////////////////////////////////////////////////////////////////
  // variant conversions
  //////////////////////////////////////////////////////////////////////////////

  void to_variant(const ip::endpoint& var, variant& vo)
  {
    vo = fc::string(var);
  }

  void from_variant(const variant& var, ip::endpoint& vo)
  {
    vo = ip::endpoint::from_string(var.as_string());
  }

  void to_variant(const ip::address& var, variant& vo)
  {
    vo = fc::string(var);
  }

  void from_variant(const variant& var, ip::address& vo)
  {
    vo = ip::address(var.as_string());
  }

  void to_variant(const ip::ipv4_address& var, variant& vo)
  {
    vo = boost::asio::ip::address_v4(var.addr).to_string().c_str();
  }

  void from_variant(const variant& var, ip::ipv4_address& vo)
  {
    vo.addr = boost::asio::ip::address_v4::from_string(var.as_string().c_str()).to_ulong();
  }

  void to_variant(const ip::ipv6_address& var, variant& vo)
  {
    boost::asio::ip::address_v6::bytes_type bytes;
    std::copy(var.addr.begin(), var.addr.end(), bytes.begin());
    vo = boost::asio::ip::address_v6(bytes).to_string().c_str();
  }

  void from_variant(const variant& var, ip::ipv6_address& vo)
  {
    auto boost_addr = boost::asio::ip::address_v6::from_string(var.as_string().c_str());
    auto bytes = boost_addr.to_bytes();
    std::copy(bytes.begin(), bytes.end(), vo.addr.begin());
  }

} // namespace fc

//////////////////////////////////////////////////////////////////////////////
// std::hash implementations
//////////////////////////////////////////////////////////////////////////////

namespace std
{
  size_t hash<fc::ip::endpoint>::operator()(const fc::ip::endpoint& e) const
  {
    size_t seed = 0;
    // Hash the address
    seed ^= hash<fc::ip::address>()(e.get_address());
    // Hash the port
    seed ^= std::hash<uint16_t>()(e.port()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  }

  size_t hash<fc::ip::address>::operator()(const fc::ip::address& a) const
  {
    if (a.is_ipv4()) {
      return std::hash<uint32_t>()(a.get_ipv4().addr);
    } else {
      // Hash the IPv6 address bytes
      const auto& v6 = a.get_ipv6();
      size_t seed = 0;
      for (int i = 0; i < 16; ++i) {
        seed ^= std::hash<uint8_t>()(v6.addr[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
    }
  }
}

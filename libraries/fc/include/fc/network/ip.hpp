#pragma once
#include <fc/string.hpp>
#include <fc/crypto/sha1.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/crypto/city.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/static_variant.hpp>
#include <fc/array.hpp>
#include <array>
#include <cstdint>

namespace fc {

  namespace ip {

    /**
     * @brief Represents an IPv4 address (32 bits)
     */
    struct ipv4_address {
      uint32_t addr = 0;

      ipv4_address() = default;
      explicit ipv4_address(uint32_t a) : addr(a) {}

      bool operator==(const ipv4_address& other) const { return addr == other.addr; }
      bool operator!=(const ipv4_address& other) const { return addr != other.addr; }
      bool operator<(const ipv4_address& other) const { return addr < other.addr; }
    };

    /**
     * @brief Represents an IPv6 address (128 bits)
     */
    struct ipv6_address {
      fc::array<uint8_t, 16> addr;

      ipv6_address() = default;
      explicit ipv6_address(const std::array<uint8_t, 16>& a) {
        std::copy(a.begin(), a.end(), addr.begin());
      }

      bool operator==(const ipv6_address& other) const { return addr == other.addr; }
      bool operator!=(const ipv6_address& other) const { return addr != other.addr; }
      bool operator<(const ipv6_address& other) const { return addr < other.addr; }
    };

    /**
     * @brief Represents an IP address that can be either IPv4 or IPv6.
     *
     * This class uses fc::static_variant internally to store either an IPv4
     * or IPv6 address. It provides a unified interface for both address types.
     */
    class address {
      public:
        /**
         * @brief Constructs a default IPv4 address (0.0.0.0)
         */
        address();

        /**
         * @brief Constructs an IPv4 address from a 32-bit integer.
         * @param ip The IPv4 address in host byte order.
         *
         * This constructor is provided for backward compatibility with
         * existing code that uses uint32_t for IPv4 addresses.
         */
        explicit address(uint32_t ip);

        /**
         * @brief Constructs an address from a string.
         * @param s The address string (IPv4 like "192.168.1.1" or IPv6 like "2001:db8::1")
         */
        explicit address(const fc::string& s);

        /**
         * @brief Constructs an IPv4 address from an ipv4_address struct.
         */
        address(const ipv4_address& v4);

        /**
         * @brief Constructs an IPv6 address from an ipv6_address struct.
         */
        address(const ipv6_address& v6);

        /**
         * @brief Assigns an address from a string.
         */
        address& operator=(const fc::string& s);

        /**
         * @brief Converts to string representation.
         *
         * For IPv4: "192.168.1.1"
         * For IPv6: "2001:db8::1"
         */
        operator fc::string() const;

        /**
         * @brief Converts to uint32_t for backward compatibility.
         * @throws fc::exception if the address is IPv6
         *
         * This operator is provided for backward compatibility. It will throw
         * if called on an IPv6 address.
         */
        operator uint32_t() const;

        /**
         * @brief Returns true if this is an IPv4 address.
         */
        bool is_ipv4() const;

        /**
         * @brief Returns true if this is an IPv6 address.
         */
        bool is_ipv6() const;

        /**
         * @brief Returns the IPv4 address.
         * @throws fc::exception if the address is IPv6
         */
        const ipv4_address& get_ipv4() const;

        /**
         * @brief Returns the IPv6 address.
         * @throws fc::exception if the address is IPv4
         */
        const ipv6_address& get_ipv6() const;

        friend bool operator==(const address& a, const address& b);
        friend bool operator!=(const address& a, const address& b);
        friend bool operator<(const address& a, const address& b);

        /**
         * @brief Checks if this is a private/local address.
         *
         * For IPv4:
         *   10.0.0.0    to 10.255.255.255
         *   172.16.0.0  to 172.31.255.255
         *   192.168.0.0 to 192.168.255.255
         *   169.254.0.0 to 169.254.255.255
         *
         * For IPv6:
         *   fc00::/7 (Unique Local Addresses)
         *   fe80::/10 (Link-local)
         */
        bool is_private_address() const;

        /**
         * @brief Checks if this is a multicast address.
         *
         * For IPv4: 224.0.0.0 to 239.255.255.255
         * For IPv6: ff00::/8
         */
        bool is_multicast_address() const;

        /**
         * @brief Checks if this is a loopback address.
         *
         * For IPv4: 127.0.0.0/8
         * For IPv6: ::1
         */
        bool is_loopback_address() const;

        /**
         * @brief Returns true if public (!private && !multicast && !loopback).
         */
        bool is_public_address() const;

      private:
        fc::static_variant<ipv4_address, ipv6_address> _addr;
    };

    class endpoint {
      public:
        endpoint();
        endpoint(const address& i, uint16_t p = 0);

        /**
         * @brief Constructs an endpoint from a string address and port.
         * @param addr_str The address string (IPv4 or IPv6)
         * @param p The port number
         *
         * This is a convenience constructor for backward compatibility.
         */
        endpoint(const fc::string& addr_str, uint16_t p);

        /**
         * @brief Converts "IP:PORT" or "[IPv6]:PORT" to an endpoint.
         *
         * IPv4 format: "192.168.1.1:8080"
         * IPv6 format: "[2001:db8::1]:8080"
         */
        static endpoint from_string(const string& s);

        /**
         * @brief Returns "IP:PORT" or "[IPv6]:PORT".
         */
        operator string() const;

        void set_port(uint16_t p) { _port = p; }
        uint16_t port() const;
        const address& get_address() const;

        friend bool operator==(const endpoint& a, const endpoint& b);
        friend bool operator!=(const endpoint& a, const endpoint& b);
        friend bool operator<(const endpoint& a, const endpoint& b);

      private:
        /**
         * The compiler pads endpoint, so we use 32 bits for port
         * to ensure consistent memory layout for hashing/memcmp.
         */
        uint32_t _port = 0;
        address  _ip;
    };

  } // namespace ip

  class variant;
  void to_variant(const ip::endpoint& var, variant& vo);
  void from_variant(const variant& var, ip::endpoint& vo);

  void to_variant(const ip::address& var, variant& vo);
  void from_variant(const variant& var, ip::address& vo);

  void to_variant(const ip::ipv4_address& var, variant& vo);
  void from_variant(const variant& var, ip::ipv4_address& vo);

  void to_variant(const ip::ipv6_address& var, variant& vo);
  void from_variant(const variant& var, ip::ipv6_address& vo);

  namespace raw
  {
    // Pack/unpack for ipv4_address
    template<typename Stream>
    inline void pack(Stream& s, const ip::ipv4_address& v)
    {
      fc::raw::pack(s, v.addr);
    }

    template<typename Stream>
    inline void unpack(Stream& s, ip::ipv4_address& v, uint32_t depth, bool limit_is_disabled)
    {
      depth++;
      fc::raw::unpack(s, v.addr, depth);
    }

    // Pack/unpack for ipv6_address
    template<typename Stream>
    inline void pack(Stream& s, const ip::ipv6_address& v)
    {
      fc::raw::pack(s, v.addr);
    }

    template<typename Stream>
    inline void unpack(Stream& s, ip::ipv6_address& v, uint32_t depth, bool limit_is_disabled)
    {
      depth++;
      fc::raw::unpack(s, v.addr, depth);
    }

    // Pack/unpack for address
    // New format: 1 byte family (0x04 or 0x06) + address bytes
    template<typename Stream>
    inline void pack(Stream& s, const ip::address& v)
    {
      if (v.is_ipv4()) {
        fc::raw::pack(s, uint8_t(0x04));
        fc::raw::pack(s, v.get_ipv4());
      } else {
        fc::raw::pack(s, uint8_t(0x06));
        fc::raw::pack(s, v.get_ipv6());
      }
    }

    template<typename Stream>
    inline void unpack(Stream& s, ip::address& v, uint32_t depth, bool limit_is_disabled)
    {
      depth++;
      uint8_t family;
      fc::raw::unpack(s, family, depth);
      if (family == 0x04) {
        ip::ipv4_address v4;
        fc::raw::unpack(s, v4, depth);
        v = ip::address(v4);
      } else if (family == 0x06) {
        ip::ipv6_address v6;
        fc::raw::unpack(s, v6, depth);
        v = ip::address(v6);
      } else {
        FC_THROW_EXCEPTION(fc::parse_error_exception, "Invalid IP address family: ${f}", ("f", int(family)));
      }
    }

    // Pack/unpack for endpoint
    template<typename Stream>
    inline void pack(Stream& s, const ip::endpoint& v)
    {
      fc::raw::pack(s, v.get_address());
      fc::raw::pack(s, v.port());
    }

    template<typename Stream>
    inline void unpack(Stream& s, ip::endpoint& v, uint32_t depth, bool limit_is_disabled)
    {
      depth++;
      ip::address a;
      uint16_t p;
      fc::raw::unpack(s, a, depth);
      fc::raw::unpack(s, p, depth);
      v = ip::endpoint(a, p);
    }

  } // namespace raw

} // namespace fc

FC_REFLECT(fc::ip::ipv4_address, (addr))
FC_REFLECT(fc::ip::ipv6_address, (addr))
FC_REFLECT_TYPENAME(fc::ip::address)
FC_REFLECT_TYPENAME(fc::ip::endpoint)

namespace std
{
  template<>
  struct hash<fc::ip::endpoint>
  {
    size_t operator()(const fc::ip::endpoint& e) const;
  };

  template<>
  struct hash<fc::ip::address>
  {
    size_t operator()(const fc::ip::address& a) const;
  };
}

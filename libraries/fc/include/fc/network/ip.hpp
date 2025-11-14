#pragma once
#include <fc/string.hpp>
#include <fc/crypto/sha1.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/crypto/city.hpp>
#include <fc/reflect/reflect.hpp>

namespace fc {

  namespace ip {
    class address {
      public:
        address( uint32_t _ip = 0 );
        address( const fc::string& s );
#ifdef ENABLE_IPV6
        bool is_v4() const { return !_is_v6; }
        bool is_v6() const { return _is_v6; }

        void set_v4(uint32_t v) {
          _is_v6 = false;
          _ip_v4 = v;
        }

        void set_v6(const std::array<uint8_t,16>& v6) {
          _is_v6 = true;
          _ip_v6 = v6;
        }

        uint32_t to_v4() const {
          return _ip_v4;
        }

        std::array<uint8_t,16> to_v6() const {
          return _ip_v6;
        }

#endif
        address& operator=( const fc::string& s );
        operator fc::string()const;
        operator uint32_t()const;

        friend bool operator==( const address& a, const address& b );
        friend bool operator!=( const address& a, const address& b );

        /**
         *  @return true if the ip is in the following ranges:
         *
         *  10.0.0.0    to 10.255.255.255
         *  172.16.0.0  to 172.31.255.255
         *  192.168.0.0 to 192.168.255.255
         *  169.254.0.0 to 169.254.255.255
         *
         */
        bool is_private_address()const;
        /**
         *  224.0.0.0 to 239.255.255.255
         */
        bool is_multicast_address()const;

        /** !private & !multicast */
        bool is_public_address()const;
      private:
#ifdef ENABLE_IPV6
        bool     _is_v6 = false;
        uint32_t _ip_v4 = 0;
        std::array<uint8_t, 16> _ip_v6{};
#else
        uint32_t _ip;
#endif
    };

    class endpoint {
      public:
        endpoint();
        endpoint( const address& i, uint16_t p = 0);
#ifdef ENABLE_IPV6
        void set_address_v4(uint32_t v4) {
          _ip.set_v4(v4);
        }

        void set_address_v6(const std::array<uint8_t, 16>& v6) {
          _ip.set_v6(v6);
        }
#endif
        /** Converts "IP:PORT" to an endpoint */
        static endpoint from_string( const string& s );
        /** returns "IP:PORT" */
        operator string()const;

        void           set_port(uint16_t p ) { _port = p; }
        uint16_t       port()const;
        const address& get_address()const;

        friend bool operator==( const endpoint& a, const endpoint& b );
        friend bool operator!=( const endpoint& a, const endpoint& b );
        friend bool operator< ( const endpoint& a, const endpoint& b );

      private:
        /**
         *  The compiler pads endpoint to a full 8 bytes, so while
         *  a port number is limited in range to 16 bits, we specify
         *  a full 32 bits so that memcmp can be used with sizeof(),
         *  otherwise 2 bytes will be 'random' and you do not know
         *  where they are stored.
         */
        uint32_t _port;
        address  _ip;
    };

  }
  class variant;
  void to_variant( const ip::endpoint& var,  variant& vo );
  void from_variant( const variant& var,  ip::endpoint& vo );

  void to_variant( const ip::address& var,  variant& vo );
  void from_variant( const variant& var,  ip::address& vo );


  namespace raw
  {
    template<typename Stream>
    inline void pack( Stream& s, const ip::address& v )
    {
       fc::raw::pack( s, uint32_t(v) );
    }
    template<typename Stream>
    inline void unpack( Stream& s, ip::address& v, uint32_t depth, bool limit_is_disabled )
    {
       depth++;
       uint32_t _ip;
       fc::raw::unpack( s, _ip, depth );
       v = ip::address(_ip);
    }

    template<typename Stream>
    inline void pack( Stream& s, const ip::endpoint& v )
    {
       fc::raw::pack( s, v.get_address() );
       fc::raw::pack( s, v.port() );
    }
    template<typename Stream>
    inline void unpack( Stream& s, ip::endpoint& v, uint32_t depth, bool limit_is_disabled )
    {
       depth++;
       ip::address a;
       uint16_t p;
       fc::raw::unpack( s, a, depth );
       fc::raw::unpack( s, p, depth );
       v = ip::endpoint(a,p);
    }

  }
} // namespace fc
FC_REFLECT_TYPENAME( fc::ip::address )
FC_REFLECT_TYPENAME( fc::ip::endpoint )
namespace std
{
    template<>
    struct hash<fc::ip::endpoint>
    {
       size_t operator()( const fc::ip::endpoint& e )const
       {
           return fc::city_hash_size_t( (char*)&e, sizeof(e) );
       }
    };
}

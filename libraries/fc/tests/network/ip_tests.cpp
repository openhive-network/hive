#include <boost/test/unit_test.hpp>

#include <fc/network/ip.hpp>
#include <fc/io/raw.hpp>
#include <fc/variant.hpp>
#include <fc/io/json.hpp>

BOOST_AUTO_TEST_SUITE(fc_ip)

//////////////////////////////////////////////////////////////////////////////
// IPv4 Address Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( ipv4_address_construction )
{
   // Default construction
   fc::ip::address addr1;
   BOOST_CHECK( addr1.is_ipv4() );
   BOOST_CHECK( !addr1.is_ipv6() );
   BOOST_CHECK_EQUAL( static_cast<uint32_t>(addr1), 0u );

   // Construction from uint32_t
   fc::ip::address addr2(0x7F000001); // 127.0.0.1
   BOOST_CHECK( addr2.is_ipv4() );
   BOOST_CHECK_EQUAL( static_cast<uint32_t>(addr2), 0x7F000001u );

   // Construction from string
   fc::ip::address addr3("192.168.1.1");
   BOOST_CHECK( addr3.is_ipv4() );
   BOOST_CHECK_EQUAL( fc::string(addr3), "192.168.1.1" );

   fc::ip::address addr4("10.0.0.1");
   BOOST_CHECK( addr4.is_ipv4() );
   BOOST_CHECK_EQUAL( fc::string(addr4), "10.0.0.1" );

   fc::ip::address addr5("0.0.0.0");
   BOOST_CHECK( addr5.is_ipv4() );
   BOOST_CHECK_EQUAL( static_cast<uint32_t>(addr5), 0u );

   fc::ip::address addr6("255.255.255.255");
   BOOST_CHECK( addr6.is_ipv4() );
   BOOST_CHECK_EQUAL( static_cast<uint32_t>(addr6), 0xFFFFFFFFu );
}

BOOST_AUTO_TEST_CASE( ipv4_address_classification )
{
   // Loopback addresses (127.0.0.0/8)
   BOOST_CHECK( fc::ip::address("127.0.0.1").is_loopback_address() );
   BOOST_CHECK( fc::ip::address("127.255.255.255").is_loopback_address() );
   BOOST_CHECK( !fc::ip::address("128.0.0.1").is_loopback_address() );

   // Private addresses
   // 10.0.0.0/8
   BOOST_CHECK( fc::ip::address("10.0.0.1").is_private_address() );
   BOOST_CHECK( fc::ip::address("10.255.255.255").is_private_address() );
   BOOST_CHECK( !fc::ip::address("11.0.0.1").is_private_address() );

   // 172.16.0.0/12
   BOOST_CHECK( fc::ip::address("172.16.0.1").is_private_address() );
   BOOST_CHECK( fc::ip::address("172.31.255.255").is_private_address() );
   BOOST_CHECK( !fc::ip::address("172.32.0.1").is_private_address() );

   // 192.168.0.0/16
   BOOST_CHECK( fc::ip::address("192.168.0.1").is_private_address() );
   BOOST_CHECK( fc::ip::address("192.168.255.255").is_private_address() );
   BOOST_CHECK( !fc::ip::address("192.169.0.1").is_private_address() );

   // 169.254.0.0/16 (link-local)
   BOOST_CHECK( fc::ip::address("169.254.0.1").is_private_address() );
   BOOST_CHECK( fc::ip::address("169.254.255.255").is_private_address() );

   // Multicast addresses (224.0.0.0/4)
   BOOST_CHECK( fc::ip::address("224.0.0.1").is_multicast_address() );
   BOOST_CHECK( fc::ip::address("239.255.255.255").is_multicast_address() );
   BOOST_CHECK( !fc::ip::address("240.0.0.1").is_multicast_address() );

   // Public addresses
   BOOST_CHECK( fc::ip::address("8.8.8.8").is_public_address() );
   BOOST_CHECK( fc::ip::address("1.1.1.1").is_public_address() );
   BOOST_CHECK( !fc::ip::address("10.0.0.1").is_public_address() );
   BOOST_CHECK( !fc::ip::address("127.0.0.1").is_public_address() );
}

BOOST_AUTO_TEST_CASE( ipv4_address_comparison )
{
   fc::ip::address a("192.168.1.1");
   fc::ip::address b("192.168.1.1");
   fc::ip::address c("192.168.1.2");

   BOOST_CHECK( a == b );
   BOOST_CHECK( !(a != b) );
   BOOST_CHECK( a != c );
   BOOST_CHECK( a < c );
   BOOST_CHECK( !(c < a) );
}

//////////////////////////////////////////////////////////////////////////////
// IPv6 Address Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( ipv6_address_construction )
{
   // Construction from string - loopback
   fc::ip::address addr1("::1");
   BOOST_CHECK( addr1.is_ipv6() );
   BOOST_CHECK( !addr1.is_ipv4() );
   BOOST_CHECK_EQUAL( fc::string(addr1), "::1" );

   // Full IPv6 address
   fc::ip::address addr2("2001:db8::1");
   BOOST_CHECK( addr2.is_ipv6() );

   // All zeros
   fc::ip::address addr3("::");
   BOOST_CHECK( addr3.is_ipv6() );
   BOOST_CHECK_EQUAL( fc::string(addr3), "::" );

   // Full form
   fc::ip::address addr4("2001:0db8:0000:0000:0000:0000:0000:0001");
   BOOST_CHECK( addr4.is_ipv6() );

   // Link-local
   fc::ip::address addr5("fe80::1");
   BOOST_CHECK( addr5.is_ipv6() );
}

BOOST_AUTO_TEST_CASE( ipv6_address_classification )
{
   // Loopback (::1)
   BOOST_CHECK( fc::ip::address("::1").is_loopback_address() );
   BOOST_CHECK( !fc::ip::address("::2").is_loopback_address() );
   BOOST_CHECK( !fc::ip::address("::").is_loopback_address() );

   // Private/Local addresses
   // fc00::/7 (Unique Local Addresses)
   BOOST_CHECK( fc::ip::address("fc00::1").is_private_address() );
   BOOST_CHECK( fc::ip::address("fd00::1").is_private_address() );
   BOOST_CHECK( !fc::ip::address("fe00::1").is_private_address() );

   // fe80::/10 (Link-local)
   BOOST_CHECK( fc::ip::address("fe80::1").is_private_address() );
   BOOST_CHECK( fc::ip::address("febf::1").is_private_address() );
   BOOST_CHECK( !fc::ip::address("fec0::1").is_private_address() );

   // Multicast (ff00::/8)
   BOOST_CHECK( fc::ip::address("ff00::1").is_multicast_address() );
   BOOST_CHECK( fc::ip::address("ff02::1").is_multicast_address() );
   BOOST_CHECK( !fc::ip::address("fe00::1").is_multicast_address() );

   // Public addresses
   BOOST_CHECK( fc::ip::address("2001:db8::1").is_public_address() );
   BOOST_CHECK( !fc::ip::address("::1").is_public_address() );
   BOOST_CHECK( !fc::ip::address("fc00::1").is_public_address() );
   BOOST_CHECK( !fc::ip::address("ff02::1").is_public_address() );
}

BOOST_AUTO_TEST_CASE( ipv6_address_comparison )
{
   fc::ip::address a("2001:db8::1");
   fc::ip::address b("2001:db8::1");
   fc::ip::address c("2001:db8::2");

   BOOST_CHECK( a == b );
   BOOST_CHECK( !(a != b) );
   BOOST_CHECK( a != c );
   BOOST_CHECK( a < c );
   BOOST_CHECK( !(c < a) );

   // IPv4 and IPv6 are never equal
   fc::ip::address v4("127.0.0.1");
   fc::ip::address v6("::1");
   BOOST_CHECK( v4 != v6 );

   // IPv4 sorts before IPv6
   BOOST_CHECK( v4 < v6 );
}

BOOST_AUTO_TEST_CASE( ipv6_uint32_conversion_throws )
{
   fc::ip::address v6("::1");
   BOOST_CHECK_THROW( static_cast<uint32_t>(v6), fc::exception );
}

//////////////////////////////////////////////////////////////////////////////
// Endpoint Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( endpoint_ipv4_construction )
{
   // Default construction
   fc::ip::endpoint ep1;
   BOOST_CHECK_EQUAL( ep1.port(), 0u );
   BOOST_CHECK( ep1.get_address().is_ipv4() );

   // From address and port
   fc::ip::address addr("192.168.1.1");
   fc::ip::endpoint ep2(addr, 8080);
   BOOST_CHECK_EQUAL( ep2.port(), 8080u );
   BOOST_CHECK_EQUAL( fc::string(ep2.get_address()), "192.168.1.1" );

   // From string
   fc::ip::endpoint ep3 = fc::ip::endpoint::from_string("10.0.0.1:9090");
   BOOST_CHECK_EQUAL( ep3.port(), 9090u );
   BOOST_CHECK_EQUAL( fc::string(ep3.get_address()), "10.0.0.1" );

   // String conversion
   BOOST_CHECK_EQUAL( fc::string(ep3), "10.0.0.1:9090" );
}

BOOST_AUTO_TEST_CASE( endpoint_ipv6_construction )
{
   // From address and port
   fc::ip::address addr("2001:db8::1");
   fc::ip::endpoint ep1(addr, 8080);
   BOOST_CHECK_EQUAL( ep1.port(), 8080u );
   BOOST_CHECK( ep1.get_address().is_ipv6() );

   // From bracketed string
   fc::ip::endpoint ep2 = fc::ip::endpoint::from_string("[::1]:9090");
   BOOST_CHECK_EQUAL( ep2.port(), 9090u );
   BOOST_CHECK( ep2.get_address().is_ipv6() );
   BOOST_CHECK_EQUAL( fc::string(ep2.get_address()), "::1" );

   // String conversion uses brackets
   fc::ip::endpoint ep3(fc::ip::address("2001:db8::1"), 8080);
   BOOST_CHECK_EQUAL( fc::string(ep3), "[2001:db8::1]:8080" );

   // Full IPv6 in brackets
   fc::ip::endpoint ep4 = fc::ip::endpoint::from_string("[2001:db8:85a3::8a2e:370:7334]:443");
   BOOST_CHECK_EQUAL( ep4.port(), 443u );
   BOOST_CHECK( ep4.get_address().is_ipv6() );
}

BOOST_AUTO_TEST_CASE( endpoint_comparison )
{
   fc::ip::endpoint ep1 = fc::ip::endpoint::from_string("192.168.1.1:8080");
   fc::ip::endpoint ep2 = fc::ip::endpoint::from_string("192.168.1.1:8080");
   fc::ip::endpoint ep3 = fc::ip::endpoint::from_string("192.168.1.1:8081");
   fc::ip::endpoint ep4 = fc::ip::endpoint::from_string("192.168.1.2:8080");

   BOOST_CHECK( ep1 == ep2 );
   BOOST_CHECK( ep1 != ep3 ); // Different port
   BOOST_CHECK( ep1 != ep4 ); // Different address
   BOOST_CHECK( ep1 < ep3 );  // Same address, lower port
   BOOST_CHECK( ep1 < ep4 );  // Lower address

   // IPv4 vs IPv6 endpoints
   fc::ip::endpoint v4ep = fc::ip::endpoint::from_string("127.0.0.1:80");
   fc::ip::endpoint v6ep = fc::ip::endpoint::from_string("[::1]:80");
   BOOST_CHECK( v4ep != v6ep );
   BOOST_CHECK( v4ep < v6ep ); // IPv4 sorts before IPv6
}

BOOST_AUTO_TEST_CASE( endpoint_set_port )
{
   fc::ip::endpoint ep = fc::ip::endpoint::from_string("192.168.1.1:8080");
   BOOST_CHECK_EQUAL( ep.port(), 8080u );

   ep.set_port(9090);
   BOOST_CHECK_EQUAL( ep.port(), 9090u );
}

//////////////////////////////////////////////////////////////////////////////
// Serialization Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( ipv4_address_serialization )
{
   fc::ip::address original("192.168.1.100");

   // Pack
   std::vector<char> buffer;
   {
      fc::datastream<size_t> ps;
      fc::raw::pack(ps, original);
      buffer.resize(ps.tellp());
   }
   {
      fc::datastream<char*> ds(buffer.data(), buffer.size());
      fc::raw::pack(ds, original);
   }

   // Unpack
   fc::ip::address unpacked;
   {
      fc::datastream<const char*> ds(buffer.data(), buffer.size());
      fc::raw::unpack(ds, unpacked);
   }

   BOOST_CHECK( unpacked.is_ipv4() );
   BOOST_CHECK( original == unpacked );
   BOOST_CHECK_EQUAL( fc::string(original), fc::string(unpacked) );
}

BOOST_AUTO_TEST_CASE( ipv6_address_serialization )
{
   fc::ip::address original("2001:db8:85a3::8a2e:370:7334");

   // Pack
   std::vector<char> buffer;
   {
      fc::datastream<size_t> ps;
      fc::raw::pack(ps, original);
      buffer.resize(ps.tellp());
   }
   {
      fc::datastream<char*> ds(buffer.data(), buffer.size());
      fc::raw::pack(ds, original);
   }

   // Unpack
   fc::ip::address unpacked;
   {
      fc::datastream<const char*> ds(buffer.data(), buffer.size());
      fc::raw::unpack(ds, unpacked);
   }

   BOOST_CHECK( unpacked.is_ipv6() );
   BOOST_CHECK( original == unpacked );
   BOOST_CHECK_EQUAL( fc::string(original), fc::string(unpacked) );
}

BOOST_AUTO_TEST_CASE( endpoint_serialization )
{
   // IPv4 endpoint
   {
      fc::ip::endpoint original = fc::ip::endpoint::from_string("192.168.1.100:8080");

      std::vector<char> buffer;
      {
         fc::datastream<size_t> ps;
         fc::raw::pack(ps, original);
         buffer.resize(ps.tellp());
      }
      {
         fc::datastream<char*> ds(buffer.data(), buffer.size());
         fc::raw::pack(ds, original);
      }

      fc::ip::endpoint unpacked;
      {
         fc::datastream<const char*> ds(buffer.data(), buffer.size());
         fc::raw::unpack(ds, unpacked);
      }

      BOOST_CHECK( original == unpacked );
      BOOST_CHECK_EQUAL( fc::string(original), fc::string(unpacked) );
   }

   // IPv6 endpoint
   {
      fc::ip::endpoint original = fc::ip::endpoint::from_string("[2001:db8::1]:443");

      std::vector<char> buffer;
      {
         fc::datastream<size_t> ps;
         fc::raw::pack(ps, original);
         buffer.resize(ps.tellp());
      }
      {
         fc::datastream<char*> ds(buffer.data(), buffer.size());
         fc::raw::pack(ds, original);
      }

      fc::ip::endpoint unpacked;
      {
         fc::datastream<const char*> ds(buffer.data(), buffer.size());
         fc::raw::unpack(ds, unpacked);
      }

      BOOST_CHECK( original == unpacked );
      BOOST_CHECK( unpacked.get_address().is_ipv6() );
      BOOST_CHECK_EQUAL( fc::string(original), fc::string(unpacked) );
   }
}

BOOST_AUTO_TEST_CASE( address_wire_format_size )
{
   // IPv4: 1 byte family + 4 bytes address = 5 bytes
   fc::ip::address v4("192.168.1.1");
   {
      fc::datastream<size_t> ps;
      fc::raw::pack(ps, v4);
      BOOST_CHECK_EQUAL( ps.tellp(), 5u );
   }

   // IPv6: 1 byte family + 16 bytes address = 17 bytes
   fc::ip::address v6("2001:db8::1");
   {
      fc::datastream<size_t> ps;
      fc::raw::pack(ps, v6);
      BOOST_CHECK_EQUAL( ps.tellp(), 17u );
   }
}

//////////////////////////////////////////////////////////////////////////////
// Variant/JSON Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( address_variant_conversion )
{
   // IPv4
   {
      fc::ip::address original("192.168.1.100");
      fc::variant v;
      fc::to_variant(original, v);
      BOOST_CHECK_EQUAL( v.as_string(), "192.168.1.100" );

      fc::ip::address restored;
      fc::from_variant(v, restored);
      BOOST_CHECK( original == restored );
   }

   // IPv6
   {
      fc::ip::address original("2001:db8::1");
      fc::variant v;
      fc::to_variant(original, v);
      BOOST_CHECK_EQUAL( v.as_string(), "2001:db8::1" );

      fc::ip::address restored;
      fc::from_variant(v, restored);
      BOOST_CHECK( original == restored );
   }
}

BOOST_AUTO_TEST_CASE( endpoint_variant_conversion )
{
   // IPv4
   {
      fc::ip::endpoint original = fc::ip::endpoint::from_string("192.168.1.100:8080");
      fc::variant v;
      fc::to_variant(original, v);
      BOOST_CHECK_EQUAL( v.as_string(), "192.168.1.100:8080" );

      fc::ip::endpoint restored;
      fc::from_variant(v, restored);
      BOOST_CHECK( original == restored );
   }

   // IPv6
   {
      fc::ip::endpoint original = fc::ip::endpoint::from_string("[2001:db8::1]:443");
      fc::variant v;
      fc::to_variant(original, v);
      BOOST_CHECK_EQUAL( v.as_string(), "[2001:db8::1]:443" );

      fc::ip::endpoint restored;
      fc::from_variant(v, restored);
      BOOST_CHECK( original == restored );
   }
}

//////////////////////////////////////////////////////////////////////////////
// Hash Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( address_hash )
{
   std::hash<fc::ip::address> hasher;

   fc::ip::address a1("192.168.1.1");
   fc::ip::address a2("192.168.1.1");
   fc::ip::address a3("192.168.1.2");

   // Same addresses should have same hash
   BOOST_CHECK_EQUAL( hasher(a1), hasher(a2) );

   // Different addresses should (very likely) have different hashes
   BOOST_CHECK( hasher(a1) != hasher(a3) );

   // IPv6
   fc::ip::address v6_1("2001:db8::1");
   fc::ip::address v6_2("2001:db8::1");
   fc::ip::address v6_3("2001:db8::2");

   BOOST_CHECK_EQUAL( hasher(v6_1), hasher(v6_2) );
   BOOST_CHECK( hasher(v6_1) != hasher(v6_3) );

   // IPv4 and IPv6 should have different hashes
   BOOST_CHECK( hasher(a1) != hasher(v6_1) );
}

BOOST_AUTO_TEST_CASE( endpoint_hash )
{
   std::hash<fc::ip::endpoint> hasher;

   fc::ip::endpoint e1 = fc::ip::endpoint::from_string("192.168.1.1:8080");
   fc::ip::endpoint e2 = fc::ip::endpoint::from_string("192.168.1.1:8080");
   fc::ip::endpoint e3 = fc::ip::endpoint::from_string("192.168.1.1:8081");
   fc::ip::endpoint e4 = fc::ip::endpoint::from_string("192.168.1.2:8080");

   // Same endpoints should have same hash
   BOOST_CHECK_EQUAL( hasher(e1), hasher(e2) );

   // Different port or address should have different hash
   BOOST_CHECK( hasher(e1) != hasher(e3) );
   BOOST_CHECK( hasher(e1) != hasher(e4) );
}

//////////////////////////////////////////////////////////////////////////////
// Error Handling Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( invalid_address_parsing )
{
   BOOST_CHECK_THROW( fc::ip::address("not_an_ip"), fc::exception );
   BOOST_CHECK_THROW( fc::ip::address("256.1.1.1"), fc::exception );
   BOOST_CHECK_THROW( fc::ip::address("1.2.3.4.5"), fc::exception );
   BOOST_CHECK_THROW( fc::ip::address(""), fc::exception );
}

BOOST_AUTO_TEST_CASE( invalid_endpoint_parsing )
{
   // Missing port
   BOOST_CHECK_THROW( fc::ip::endpoint::from_string("192.168.1.1"), fc::exception );

   // Missing closing bracket
   BOOST_CHECK_THROW( fc::ip::endpoint::from_string("[::1:8080"), fc::exception );

   // Invalid port
   BOOST_CHECK_THROW( fc::ip::endpoint::from_string("192.168.1.1:99999"), fc::exception );
   BOOST_CHECK_THROW( fc::ip::endpoint::from_string("192.168.1.1:abc"), fc::exception );
}

BOOST_AUTO_TEST_SUITE_END()

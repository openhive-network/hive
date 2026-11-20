#include <boost/test/unit_test.hpp>

#include <graphene/net/node.hpp>

#include <algorithm>
#include <string>
#include <vector>

using graphene::net::connection_candidate;
using graphene::net::connection_host_of;
using graphene::net::connection_selection_limits;
using graphene::net::exceeds_connections_per_host;
using graphene::net::get_connection_spread;
using graphene::net::select_connection_candidates;

namespace
{
  fc::ip::address address( const std::string& text )
  {
    return fc::ip::address( fc::string( text ) );
  }

  fc::ip::endpoint endpoint( const std::string& text, uint16_t port )
  {
    return fc::ip::endpoint( address( text ), port );
  }

  /// Single-host candidates of one family, `count` of them, each on a host of its own
  std::vector< connection_candidate > single_hosts( bool ipv6, int count, int first = 1 )
  {
    std::vector< connection_candidate > candidates;
    for( int i = first; i < first + count; ++i )
      candidates.push_back( { ipv6 ? endpoint( "2001:db8:" + std::to_string( i ) + "::1", 2001 )
                                   : endpoint( "198.51.100." + std::to_string( i ), 2001 ) } );
    return candidates;
  }

  connection_selection_limits limits( uint32_t free_slots, uint32_t per_host, uint32_t reserved_percent, uint32_t desired = 20 )
  {
    connection_selection_limits result;
    result.free_slots = free_slots;
    result.desired_connections = desired;
    result.maximum_connections_per_host = per_host;
    result.reserved_address_family_percent = reserved_percent;
    return result;
  }

  size_t count_on_host( const std::vector< fc::ip::endpoint >& selected, const fc::ip::address& host )
  {
    return std::count_if( selected.begin(), selected.end(),
                          [&]( const fc::ip::endpoint& e ) { return connection_host_of( e.get_address() ) == host; } );
  }

  size_t count_ipv4( const std::vector< fc::ip::endpoint >& selected )
  {
    return std::count_if( selected.begin(), selected.end(), []( const fc::ip::endpoint& e ) { return e.get_address().is_ipv4(); } );
  }

  std::vector< fc::ip::address > addresses_of( const std::vector< fc::ip::endpoint >& endpoints )
  {
    std::vector< fc::ip::address > result;
    for( const fc::ip::endpoint& e : endpoints )
      result.push_back( e.get_address() );
    return result;
  }
}

BOOST_AUTO_TEST_SUITE( p2p_connection_selection_tests )

/// An IPv4 address is a host of its own; IPv6 addresses group by their /64, and an IPv4-mapped one is its IPv4 address
BOOST_AUTO_TEST_CASE( hosts_are_ipv4_addresses_and_ipv6_prefixes )
{
  BOOST_CHECK( connection_host_of( address( "51.161.87.109" ) ) == address( "51.161.87.109" ) );
  BOOST_CHECK( connection_host_of( address( "51.161.87.109" ) ) != connection_host_of( address( "51.161.87.110" ) ) );
  BOOST_CHECK( connection_host_of( address( "2a01:4f8:172:2b8f::2" ) ) == connection_host_of( address( "2a01:4f8:172:2b8f:1:2:3:4" ) ) );
  BOOST_CHECK( connection_host_of( address( "2a01:4f8:172:2b8f::2" ) ) == address( "2a01:4f8:172:2b8f::" ) );
  BOOST_CHECK( connection_host_of( address( "2a01:4f8:172:2b8f::2" ) ) != connection_host_of( address( "2a01:4f8:172:2b90::2" ) ) );
  BOOST_CHECK( connection_host_of( address( "::ffff:51.161.87.109" ) ) == address( "51.161.87.109" ) );
  BOOST_CHECK( connection_host_of( address( "::1" ) ) == address( "::1" ) );
}

/// The limit counts connections to the host, not to the address; loopback and a limit of 0 are never exceeded
BOOST_AUTO_TEST_CASE( per_host_limit )
{
  const fc::ip::address host_a = address( "2a01:4f8:172:2b8f::2" );
  const std::vector< fc::ip::address > one_held = { address( "2a01:4f8:172:2b8f::3" ), address( "23.145.40.230" ) };
  const std::vector< fc::ip::address > two_held = { address( "2a01:4f8:172:2b8f::3" ), address( "2a01:4f8:172:2b8f::2" ) };

  BOOST_CHECK( !exceeds_connections_per_host( host_a, one_held, 2 ) );
  BOOST_CHECK( exceeds_connections_per_host( host_a, two_held, 2 ) );
  BOOST_CHECK( !exceeds_connections_per_host( host_a, two_held, 3 ) );
  BOOST_CHECK( !exceeds_connections_per_host( host_a, two_held, 0 ) );
  BOOST_CHECK( !exceeds_connections_per_host( address( "2a01:4f8:172:2b90::2" ), two_held, 2 ) );

  BOOST_CHECK( exceeds_connections_per_host( address( "::ffff:23.145.40.230" ), { address( "23.145.40.230" ) }, 1 ) );

  const std::vector< fc::ip::address > loopback = { address( "127.0.0.1" ), address( "127.0.0.1" ), address( "127.0.0.1" ) };
  BOOST_CHECK( !exceeds_connections_per_host( address( "127.0.0.1" ), loopback, 2 ) );
  BOOST_CHECK( !exceeds_connections_per_host( address( "::1" ), { address( "::1" ), address( "::1" ) }, 1 ) );
}

/// One host offering 7 ports, ahead of 13 single-port hosts in the database: at most 2 of its ports are dialed,
/// and the other slots go to the other hosts
BOOST_AUTO_TEST_CASE( one_host_with_many_ports )
{
  const fc::ip::address crowded = connection_host_of( address( "2a01:4f8:172:2b8f::2" ) );
  std::vector< connection_candidate > candidates;
  for( uint16_t port : { 2154, 2188, 2193, 2291, 2425, 2520, 2543 } )
    candidates.push_back( { endpoint( "2a01:4f8:172:2b8f::2", port ) } );
  const std::vector< connection_candidate > others = single_hosts( true, 13 );
  candidates.insert( candidates.end(), others.begin(), others.end() );

  const std::vector< fc::ip::endpoint > selected = select_connection_candidates( candidates, {}, limits( 20, 2, 25 ) );
  BOOST_CHECK_EQUAL( count_on_host( selected, crowded ), 2u );
  BOOST_CHECK_EQUAL( selected.size(), 15u );
  for( const connection_candidate& other : others )
    BOOST_CHECK( std::find( selected.begin(), selected.end(), other.endpoint ) != selected.end() );
  BOOST_CHECK_EQUAL( get_connection_spread( addresses_of( selected ) ).hosts, 14u );

  // two connections to the host held already, active or handshaking: none of its ports is dialed
  const std::vector< fc::ip::address > held = { address( "2a01:4f8:172:2b8f::2" ), address( "2a01:4f8:172:2b8f::2" ) };
  BOOST_CHECK_EQUAL( count_on_host( select_connection_candidates( candidates, held, limits( 18, 2, 25 ) ), crowded ), 0u );

  // without a limit, database order decides
  BOOST_CHECK_EQUAL( count_on_host( select_connection_candidates( candidates, {}, limits( 20, 0, 25 ) ), crowded ), 7u );
}

/// A seed or add-node target is dialed whatever its host holds already, and counts against its host for the rest
BOOST_AUTO_TEST_CASE( exempt_candidates_count_against_their_host )
{
  const fc::ip::address seed_host = address( "51.161.87.109" );
  const std::vector< connection_candidate > candidates = {
    { endpoint( "51.161.87.109", 2001 ), true },
    { endpoint( "51.161.87.109", 2002 ) },
    { endpoint( "51.161.87.109", 2003 ) } };

  std::vector< fc::ip::endpoint > selected = select_connection_candidates( candidates, { seed_host, seed_host }, limits( 10, 2, 25 ) );
  BOOST_REQUIRE_EQUAL( selected.size(), 1u );
  BOOST_CHECK( selected[ 0 ] == candidates[ 0 ].endpoint );

  selected = select_connection_candidates( candidates, { seed_host }, limits( 10, 2, 25 ) );
  BOOST_REQUIRE_EQUAL( selected.size(), 1u );
  BOOST_CHECK( selected[ 0 ] == candidates[ 0 ].endpoint );

  BOOST_CHECK_EQUAL( select_connection_candidates( candidates, {}, limits( 10, 2, 25 ) ).size(), 2u );
}

/// A full reconnect with candidates of both families: IPv6 ones come first in dialing order, but IPv4 keeps its
/// reserved share (25 % of 20) rather than being left out once IPv6 fills the slots
BOOST_AUTO_TEST_CASE( dual_stack_reconnect_keeps_both_families )
{
  std::vector< connection_candidate > candidates = single_hosts( true, 30 );
  const std::vector< connection_candidate > ipv4 = single_hosts( false, 30 );
  candidates.insert( candidates.end(), ipv4.begin(), ipv4.end() );

  std::vector< fc::ip::endpoint > selected = select_connection_candidates( candidates, {}, limits( 20, 2, 25 ) );
  BOOST_REQUIRE_EQUAL( selected.size(), 20u );
  BOOST_CHECK_EQUAL( count_ipv4( selected ), 5u );
  BOOST_CHECK( selected[ 0 ].get_address().is_ipv6() );

  // the old IPv6-first order, without a reserved share, leaves IPv4 out
  BOOST_CHECK_EQUAL( count_ipv4( select_connection_candidates( candidates, {}, limits( 20, 2, 0 ) ) ), 0u );

  // a share over 50 % is taken as 50 %
  BOOST_CHECK_EQUAL( count_ipv4( select_connection_candidates( candidates, {}, limits( 20, 2, 100 ) ) ), 10u );

  // with fewer IPv4 candidates than the share, IPv6 ones take the rest
  const std::vector< connection_candidate > few_ipv4( candidates.begin(), candidates.begin() + 32 );
  selected = select_connection_candidates( few_ipv4, {}, limits( 20, 2, 25 ) );
  BOOST_CHECK_EQUAL( selected.size(), 20u );
  BOOST_CHECK_EQUAL( count_ipv4( selected ), 2u );
}

/// The share counts the connections held: one free slot with no IPv4 connection goes to IPv4, and none is
/// added to a family that holds its share already
BOOST_AUTO_TEST_CASE( reserved_share_counts_held_connections )
{
  std::vector< connection_candidate > candidates = single_hosts( true, 30, 100 );
  const std::vector< connection_candidate > ipv4 = single_hosts( false, 30, 100 );
  candidates.insert( candidates.end(), ipv4.begin(), ipv4.end() );

  std::vector< fc::ip::address > nineteen_ipv6;
  for( const connection_candidate& held : single_hosts( true, 19 ) )
    nineteen_ipv6.push_back( held.endpoint.get_address() );
  std::vector< fc::ip::endpoint > selected = select_connection_candidates( candidates, nineteen_ipv6, limits( 1, 2, 25 ) );
  BOOST_REQUIRE_EQUAL( selected.size(), 1u );
  BOOST_CHECK( selected[ 0 ].get_address().is_ipv4() );

  std::vector< fc::ip::address > five_ipv4;
  for( const connection_candidate& held : single_hosts( false, 5 ) )
    five_ipv4.push_back( held.endpoint.get_address() );
  selected = select_connection_candidates( candidates, five_ipv4, limits( 15, 2, 25 ) );
  BOOST_CHECK_EQUAL( selected.size(), 15u );
  BOOST_CHECK_EQUAL( count_ipv4( selected ), 0u );
}

/// IPv6-only candidates fill every slot: a share is kept only for a family that has candidates
BOOST_AUTO_TEST_CASE( single_family_candidates_fill_every_slot )
{
  std::vector< fc::ip::endpoint > selected = select_connection_candidates( single_hosts( true, 30 ), {}, limits( 20, 2, 25 ) );
  BOOST_CHECK_EQUAL( selected.size(), 20u );
  BOOST_CHECK_EQUAL( count_ipv4( selected ), 0u );

  selected = select_connection_candidates( single_hosts( false, 30 ), {}, limits( 20, 2, 25 ) );
  BOOST_CHECK_EQUAL( selected.size(), 20u );
  BOOST_CHECK_EQUAL( count_ipv4( selected ), 20u );
}

/// The connection log line's spread: distinct hosts, and connections per family (an IPv4-mapped address is IPv4)
BOOST_AUTO_TEST_CASE( spread_of_connections )
{
  const graphene::net::connection_spread spread = get_connection_spread( {
    address( "2a01:4f8:172:2b8f::2" ), address( "2a01:4f8:172:2b8f::3" ), address( "2607:5300:203:c38d::" ),
    address( "51.161.87.109" ), address( "::ffff:51.161.87.109" ), address( "23.145.40.230" ) } );
  BOOST_CHECK_EQUAL( spread.hosts, 4u );
  BOOST_CHECK_EQUAL( spread.ipv6, 3u );
  BOOST_CHECK_EQUAL( spread.ipv4, 3u );
}

BOOST_AUTO_TEST_SUITE_END()

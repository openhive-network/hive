#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/node.hpp>

#include <algorithm>
#include <string>
#include <vector>

using graphene::net::accepts_exchanged_address;
using graphene::net::address_subnet;
using graphene::net::connection_candidate;
using graphene::net::connection_host_of;
using graphene::net::connection_selection_limits;
using graphene::net::exceeds_connections_per_host;
using graphene::net::get_connection_spread;
using graphene::net::get_relaxed_connections_per_host;
using graphene::net::host_limited_connection;
using graphene::net::inbound_connections_share_host;
using graphene::net::is_in_any_subnet;
using graphene::net::select_connection_candidates;
using graphene::net::select_connection_over_host_limit;

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

  connection_selection_limits relaxed_limits( uint32_t free_slots, uint32_t per_host, uint32_t desired = 20 )
  {
    connection_selection_limits result = limits( free_slots, per_host, 25, desired );
    result.relax_connections_per_host = true;
    return result;
  }

  /// `count` candidates on the one IPv4 host `host`, on ports from 2001
  std::vector< connection_candidate > ports_on_host( const std::string& host, int count )
  {
    std::vector< connection_candidate > candidates;
    for( int i = 0; i < count; ++i )
      candidates.push_back( { endpoint( host, 2001 + i ) } );
    return candidates;
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

/// Every machine on a link has an address in fe80::/64, so a link-local address is a host of its own
BOOST_AUTO_TEST_CASE( link_local_addresses_are_hosts_of_their_own )
{
  BOOST_CHECK( connection_host_of( address( "fe80::1c2d:3eff:fe4f:5a6b" ) ) == address( "fe80::1c2d:3eff:fe4f:5a6b" ) );
  BOOST_CHECK( connection_host_of( address( "fe80::1c2d:3eff:fe4f:5a6b" ) ) != connection_host_of( address( "fe80::1c2d:3eff:fe4f:5a6c" ) ) );
  BOOST_CHECK( connection_host_of( address( "febf::1" ) ) == address( "febf::1" ) );
  BOOST_CHECK( connection_host_of( address( "fec0::1" ) ) == address( "fec0::" ) );

  const std::vector< fc::ip::address > two_neighbours = { address( "fe80::1" ), address( "fe80::2" ) };
  BOOST_CHECK( !exceeds_connections_per_host( address( "fe80::3" ), two_neighbours, 2 ) );
  BOOST_CHECK( exceeds_connections_per_host( address( "fe80::2" ), two_neighbours, 1 ) );
}

/// The limit counts connections to the host, not to the address; a limit of 0 is never exceeded
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
}

/// Exempt subnets: one address, a prefix of either family, an IPv4-mapped address matched as IPv4
BOOST_AUTO_TEST_CASE( exempt_subnets )
{
  const address_subnet proxy = address_subnet::from_string( "172.17.0.1" );
  BOOST_CHECK_EQUAL( proxy.prefix_length, 32u );
  BOOST_CHECK( proxy.contains( address( "172.17.0.1" ) ) );
  BOOST_CHECK( proxy.contains( address( "::ffff:172.17.0.1" ) ) );
  BOOST_CHECK( !proxy.contains( address( "172.17.0.2" ) ) );

  const address_subnet private_block = address_subnet::from_string( "10.0.0.0/8" );
  BOOST_CHECK( private_block.contains( address( "10.255.1.2" ) ) );
  BOOST_CHECK( !private_block.contains( address( "11.0.0.1" ) ) );
  BOOST_CHECK( !private_block.contains( address( "::a00:1" ) ) );

  const address_subnet odd_prefix = address_subnet::from_string( "2001:db8:8000::/33" );
  BOOST_CHECK( odd_prefix.contains( address( "2001:db8:ffff::1" ) ) );
  BOOST_CHECK( !odd_prefix.contains( address( "2001:db8:7fff::1" ) ) );
  BOOST_CHECK( !odd_prefix.contains( address( "10.0.0.1" ) ) );

  BOOST_CHECK( address_subnet::from_string( "0.0.0.0/0" ).contains( address( "198.51.100.7" ) ) );
  BOOST_CHECK( address_subnet::from_string( "::/0" ).contains( address( "2a01:4f8::1" ) ) );

  BOOST_CHECK_THROW( address_subnet::from_string( "10.0.0.0/33" ), fc::exception );
  BOOST_CHECK_THROW( address_subnet::from_string( "2001:db8::/129" ), fc::exception );
  BOOST_CHECK_THROW( address_subnet::from_string( "10.0.0.0/" ), fc::exception );
  BOOST_CHECK_THROW( address_subnet::from_string( "10.0.0.0/8x" ), fc::exception );
  BOOST_CHECK_THROW( address_subnet::from_string( "docker-proxy" ), fc::exception );
}

/// Local test networks run many nodes on loopback, which the default exempt list covers
BOOST_AUTO_TEST_CASE( loopback_exempt_by_default )
{
  std::vector< address_subnet > defaults;
  for( const std::string& subnet : graphene::net::node_configuration().host_limit_exempt_subnets )
    defaults.push_back( address_subnet::from_string( subnet ) );
  BOOST_CHECK( is_in_any_subnet( address( "127.0.0.1" ), defaults ) );
  BOOST_CHECK( is_in_any_subnet( address( "127.1.2.3" ), defaults ) );
  BOOST_CHECK( is_in_any_subnet( address( "::1" ), defaults ) );
  BOOST_CHECK( !is_in_any_subnet( address( "10.0.0.1" ), defaults ) );
  BOOST_CHECK( !is_in_any_subnet( address( "::2" ), defaults ) );
}

/// The shared-host warning: counting the refused hello, more than half of the inbound connections are on its host
BOOST_AUTO_TEST_CASE( inbound_connections_on_one_host )
{
  const fc::ip::address proxy = address( "172.17.0.1" );
  BOOST_CHECK( inbound_connections_share_host( proxy, {} ) );
  BOOST_CHECK( inbound_connections_share_host( proxy, { proxy, proxy } ) );
  BOOST_CHECK( inbound_connections_share_host( proxy, { proxy, proxy, address( "198.51.100.1" ) } ) );
  BOOST_CHECK( inbound_connections_share_host( proxy, { proxy, proxy, address( "198.51.100.1" ), address( "198.51.100.2" ) } ) );
  BOOST_CHECK( !inbound_connections_share_host( proxy, { proxy, proxy, address( "198.51.100.1" ), address( "198.51.100.2" ),
                                                         address( "198.51.100.3" ) } ) );
  BOOST_CHECK( !inbound_connections_share_host( proxy, { proxy, address( "198.51.100.1" ), address( "198.51.100.2" ),
                                                         address( "198.51.100.3" ) } ) );
  BOOST_CHECK( inbound_connections_share_host( address( "2a01:4f8:172:2b8f::9" ),
                                               { address( "2a01:4f8:172:2b8f::2" ), address( "2a01:4f8:172:2b8f::3" ), proxy } ) );
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

/// Candidates on 2 hosts only, 10 on each: the default (relaxed) limit fills all 20 slots, 10 per host, while a hard
/// limit of 2 stops at 2 per host
BOOST_AUTO_TEST_CASE( relaxed_limit_fills_slots_from_few_hosts )
{
  std::vector< connection_candidate > candidates = ports_on_host( "198.51.100.1", 10 );
  const std::vector< connection_candidate > second = ports_on_host( "198.51.100.2", 10 );
  candidates.insert( candidates.end(), second.begin(), second.end() );

  std::vector< fc::ip::endpoint > selected = select_connection_candidates( candidates, {}, relaxed_limits( 20, 2 ) );
  BOOST_CHECK_EQUAL( selected.size(), 20u );
  BOOST_CHECK_EQUAL( count_on_host( selected, address( "198.51.100.1" ) ), 10u );
  BOOST_CHECK_EQUAL( count_on_host( selected, address( "198.51.100.2" ) ), 10u );

  selected = select_connection_candidates( candidates, {}, limits( 20, 2, 25 ) );
  BOOST_CHECK_EQUAL( selected.size(), 4u );
  BOOST_CHECK_EQUAL( count_on_host( selected, address( "198.51.100.1" ) ), 2u );
  BOOST_CHECK_EQUAL( count_on_host( selected, address( "198.51.100.2" ) ), 2u );

  // relaxed, but nothing to limit with 0
  connection_selection_limits unlimited = relaxed_limits( 20, 0 );
  BOOST_CHECK_EQUAL( select_connection_candidates( candidates, {}, unlimited ).size(), 20u );
}

/// Past the limit, each slot goes to the host holding the fewest connections then, counting the ones held already
BOOST_AUTO_TEST_CASE( relaxed_limit_prefers_hosts_with_fewest_connections )
{
  std::vector< connection_candidate > candidates = ports_on_host( "198.51.100.1", 6 );
  const std::vector< connection_candidate > second = ports_on_host( "198.51.100.2", 6 );
  candidates.insert( candidates.end(), second.begin(), second.end() );
  const std::vector< fc::ip::address > held = { address( "198.51.100.1" ), address( "198.51.100.1" ), address( "198.51.100.1" ),
                                                address( "198.51.100.1" ) };

  // the 2nd host gets its 2 under the limit, then 2 more before the 1st host (holding 4) gets another
  std::vector< fc::ip::endpoint > selected = select_connection_candidates( candidates, held, relaxed_limits( 5, 2 ) );
  BOOST_REQUIRE_EQUAL( selected.size(), 5u );
  BOOST_CHECK_EQUAL( count_on_host( selected, address( "198.51.100.2" ) ), 4u );
  BOOST_CHECK_EQUAL( count_on_host( selected, address( "198.51.100.1" ) ), 1u );
  BOOST_CHECK( connection_host_of( selected.back().get_address() ) == address( "198.51.100.1" ) );

  // fewer candidates than free slots: every one of them is picked, no more
  selected = select_connection_candidates( candidates, held, relaxed_limits( 20, 2 ) );
  BOOST_CHECK_EQUAL( selected.size(), 12u );
}

/// With candidates on many hosts the relaxed limit takes none past it: it gives way only to slots that would stay empty
BOOST_AUTO_TEST_CASE( relaxed_limit_holds_with_many_hosts )
{
  std::vector< connection_candidate > candidates;
  for( int host = 1; host <= 12; ++host )
  {
    const std::vector< connection_candidate > on_host = ports_on_host( "198.51.100." + std::to_string( host ), 5 );
    candidates.insert( candidates.end(), on_host.begin(), on_host.end() );
  }
  const std::vector< fc::ip::endpoint > selected = select_connection_candidates( candidates, {}, relaxed_limits( 20, 2 ) );
  BOOST_CHECK_EQUAL( selected.size(), 20u );
  for( int host = 1; host <= 12; ++host )
    BOOST_CHECK_LE( count_on_host( selected, address( "198.51.100." + std::to_string( host ) ) ), 2u );
}

/// Check B of hive#1170: 5 hosts, 7 candidates on one of them, too few in all for the desired 20.  Relaxed, the crowded
/// host gets its share of 4, not all 7; with enough candidates on the others, only as many steps as fill the slots
BOOST_AUTO_TEST_CASE( relaxed_limit_steps_one_host_at_a_time )
{
  const fc::ip::address crowded = address( "198.51.100.1" );
  std::vector< connection_candidate > candidates = ports_on_host( "198.51.100.1", 7 );
  for( int host = 2; host <= 5; ++host )
  {
    const std::vector< connection_candidate > on_host = ports_on_host( "198.51.100." + std::to_string( host ), 1 );
    candidates.insert( candidates.end(), on_host.begin(), on_host.end() );
  }
  std::vector< fc::ip::endpoint > selected = select_connection_candidates( candidates, {}, relaxed_limits( 20, 2 ) );
  BOOST_CHECK_EQUAL( count_on_host( selected, crowded ), 4u );
  BOOST_CHECK_EQUAL( selected.size(), 8u );

  // held already: the crowded host's share is reached, so nothing more there
  selected = select_connection_candidates( candidates, { crowded, crowded, crowded, crowded }, relaxed_limits( 16, 2 ) );
  BOOST_CHECK_EQUAL( count_on_host( selected, crowded ), 0u );
  BOOST_CHECK_EQUAL( selected.size(), 4u );

  // 4 candidates on each of the 4 other hosts: step 4 fills the 20 slots, 4 on every host
  candidates = ports_on_host( "198.51.100.1", 7 );
  for( int host = 2; host <= 5; ++host )
  {
    const std::vector< connection_candidate > on_host = ports_on_host( "198.51.100." + std::to_string( host ), 4 );
    candidates.insert( candidates.end(), on_host.begin(), on_host.end() );
  }
  selected = select_connection_candidates( candidates, {}, relaxed_limits( 20, 2 ) );
  BOOST_REQUIRE_EQUAL( selected.size(), 20u );
  for( int host = 1; host <= 5; ++host )
    BOOST_CHECK_EQUAL( count_on_host( selected, address( "198.51.100." + std::to_string( host ) ) ), 4u );

  // with 7 slots past the 10 under the limit, every host gets its 3rd before any gets a 4th
  selected = select_connection_candidates( candidates, {}, relaxed_limits( 17, 2 ) );
  BOOST_REQUIRE_EQUAL( selected.size(), 17u );
  for( int host = 1; host <= 5; ++host )
    BOOST_CHECK_GE( count_on_host( selected, address( "198.51.100." + std::to_string( host ) ) ), 3u );

  // a host already a step further than the others waits for them
  const fc::ip::address other = address( "198.51.100.2" );
  selected = select_connection_candidates( { { endpoint( "198.51.100.1", 3000 ) }, { endpoint( "198.51.100.2", 3000 ) } },
                                           { crowded, crowded, crowded, other, other }, relaxed_limits( 1, 2 ) );
  BOOST_REQUIRE_EQUAL( selected.size(), 1u );
  BOOST_CHECK( connection_host_of( selected.front().get_address() ) == other );

  // a hard limit takes none of them past it
  selected = select_connection_candidates( candidates, {}, limits( 20, 2, 25 ) );
  BOOST_CHECK_EQUAL( count_on_host( selected, crowded ), 2u );
}

/// The relaxed limit: the lowest step that reaches the desired connections, or each host's share when none does
BOOST_AUTO_TEST_CASE( relaxed_connections_per_host )
{
  BOOST_CHECK_EQUAL( get_relaxed_connections_per_host( { 10, 10 }, 2, 20 ), 10u );
  BOOST_CHECK_EQUAL( get_relaxed_connections_per_host( { 1, 5 }, 2, 4 ), 3u );     // the 2-host case: 1 + 3 reach 4
  BOOST_CHECK_EQUAL( get_relaxed_connections_per_host( { 15, 5 }, 2, 20 ), 15u );
  BOOST_CHECK_EQUAL( get_relaxed_connections_per_host( { 7, 1, 1, 1, 1 }, 2, 20 ), 4u ); // short anyway: ceil(20 / 5)
  BOOST_CHECK_EQUAL( get_relaxed_connections_per_host( { 7, 3, 3, 3, 3 }, 2, 20 ), 4u );
  BOOST_CHECK_EQUAL( get_relaxed_connections_per_host( { 7, 4, 4, 4, 4 }, 2, 20 ), 4u );
  BOOST_CHECK_EQUAL( get_relaxed_connections_per_host( { 7, 7, 7 }, 2, 20 ), 7u );
  BOOST_CHECK_EQUAL( get_relaxed_connections_per_host( { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 }, 2, 20 ), 2u ); // not relaxed
  BOOST_CHECK_EQUAL( get_relaxed_connections_per_host( { 1 }, 2, 1 ), 2u ); // never below the limit
  BOOST_CHECK_EQUAL( get_relaxed_connections_per_host( {}, 2, 20 ), 2u );
  BOOST_CHECK_EQUAL( get_relaxed_connections_per_host( { 7, 1 }, 0, 20 ), 0u );
}

/// The relaxed steps look at the first GRAPHENE_NET_MAX_RELAXED_CANDIDATES_PER_PASS candidates the limit left out only:
/// one host offering far more ports than that costs a pass no more, and a candidate past them waits for a later pass
BOOST_AUTO_TEST_CASE( relaxed_limit_scans_a_bounded_number_of_candidates )
{
  const fc::ip::address crowded = address( "198.51.100.1" );
  const fc::ip::address late = address( "198.51.100.2" );
  // the 2 under the limit, then GRAPHENE_NET_MAX_RELAXED_CANDIDATES_PER_PASS left out on the same host, then one on another
  std::vector< connection_candidate > candidates = ports_on_host( "198.51.100.1", GRAPHENE_NET_MAX_RELAXED_CANDIDATES_PER_PASS + 2 );
  candidates.push_back( { endpoint( "198.51.100.2", 2001 ) } );
  // the late host has a slot under the limit: it is taken before the relaxed steps, whatever its position
  std::vector< fc::ip::endpoint > selected = select_connection_candidates( candidates, {}, relaxed_limits( 4, 2 ) );
  BOOST_REQUIRE_EQUAL( selected.size(), 4u );
  BOOST_CHECK_EQUAL( count_on_host( selected, late ), 1u );

  // held at the limit, the late host is one of the left out, past the scanned ones: the slots go to the crowded host
  selected = select_connection_candidates( candidates, { late, late }, relaxed_limits( 20, 2 ) );
  BOOST_CHECK_EQUAL( count_on_host( selected, late ), 0u );
  BOOST_CHECK_EQUAL( count_on_host( selected, crowded ), 18u );

  // among the scanned ones it is picked first, holding fewer than the crowded host by then
  std::rotate( candidates.begin() + 2, candidates.end() - 1, candidates.end() );
  selected = select_connection_candidates( candidates, { late, late }, relaxed_limits( 20, 2 ) );
  BOOST_CHECK_EQUAL( count_on_host( selected, late ), 1u );
}

/// Addresses learned by peer exchange: public ones from anyone; loopback ones from a loopback peer only; private and
/// link-local ones from a peer that is not public itself; unspecified and multicast ones from nobody
BOOST_AUTO_TEST_CASE( exchanged_addresses )
{
  const fc::ip::address public_peer = address( "198.51.100.1" );
  const fc::ip::address public_ipv6_peer = address( "2001:db8::1" );
  const fc::ip::address loopback_peer = address( "127.0.0.1" );
  const fc::ip::address private_peer = address( "10.1.2.3" );

  BOOST_CHECK( accepts_exchanged_address( address( "203.0.113.7" ), public_peer ) );
  BOOST_CHECK( accepts_exchanged_address( address( "2001:db8:1::7" ), private_peer ) );

  for( const char* loopback : { "127.0.0.1", "127.0.0.2", "::1", "::ffff:127.0.0.1" } )
  {
    BOOST_CHECK_MESSAGE( !accepts_exchanged_address( address( loopback ), public_peer ), loopback );
    BOOST_CHECK_MESSAGE( !accepts_exchanged_address( address( loopback ), public_ipv6_peer ), loopback );
    BOOST_CHECK_MESSAGE( !accepts_exchanged_address( address( loopback ), private_peer ), loopback );
    BOOST_CHECK_MESSAGE( accepts_exchanged_address( address( loopback ), loopback_peer ), loopback );
  }
  for( const char* local : { "10.0.0.5", "172.16.0.5", "192.168.1.5", "169.254.0.5", "fd00::5", "fe80::5" } )
  {
    BOOST_CHECK_MESSAGE( !accepts_exchanged_address( address( local ), public_peer ), local );
    BOOST_CHECK_MESSAGE( accepts_exchanged_address( address( local ), private_peer ), local );
    BOOST_CHECK_MESSAGE( accepts_exchanged_address( address( local ), loopback_peer ), local );
  }
  for( const char* never : { "0.0.0.0", "::", "224.0.0.1", "ff02::1" } )
    BOOST_CHECK_MESSAGE( !accepts_exchanged_address( address( never ), loopback_peer ), never );
  // a peer at an IPv4-mapped address is the IPv4 peer
  BOOST_CHECK( accepts_exchanged_address( address( "127.0.0.2" ), address( "::ffff:127.0.0.1" ) ) );
}

/// The connection that gives way to a new host: not exempt, on the host with the most connections over the limit,
/// the last one of that host; none when nothing is over the limit or there is no limit
BOOST_AUTO_TEST_CASE( connection_over_host_limit )
{
  const fc::ip::address a = address( "198.51.100.1" );
  const fc::ip::address b = address( "198.51.100.2" );
  const fc::ip::address c = address( "198.51.100.3" );

  BOOST_CHECK( !select_connection_over_host_limit( { { a }, { a }, { b }, { b }, { c } }, 2 ) );
  BOOST_CHECK( !select_connection_over_host_limit( { { a }, { a }, { a } }, 0 ) );

  fc::optional< size_t > selected = select_connection_over_host_limit( { { a }, { b }, { a }, { b }, { b }, { a }, { b }, { c } }, 2 );
  BOOST_REQUIRE( selected );
  BOOST_CHECK_EQUAL( *selected, 6u ); // b holds 4, a 3: the last of b

  // equal hosts: the first one over the limit, its last connection
  selected = select_connection_over_host_limit( { { a }, { b }, { a }, { b }, { b }, { a } }, 2 );
  BOOST_REQUIRE( selected );
  BOOST_CHECK_EQUAL( *selected, 5u );

  // an exempt connection counts against its host, but is never the one closed
  selected = select_connection_over_host_limit( { { a }, { a }, { a, true } }, 2 );
  BOOST_REQUIRE( selected );
  BOOST_CHECK_EQUAL( *selected, 1u );
  BOOST_CHECK( !select_connection_over_host_limit( { { a, true }, { a, true }, { a, true } }, 2 ) );

  // an IPv6 host is its /64
  selected = select_connection_over_host_limit( { { address( "2001:db8::1" ) }, { address( "2001:db8::2" ) }, { address( "2001:db8::3" ) } }, 2 );
  BOOST_REQUIRE( selected );
  BOOST_CHECK_EQUAL( *selected, 2u );
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

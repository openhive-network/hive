#include <boost/test/unit_test.hpp>

#include <graphene/net/node.hpp>

#include <algorithm>
#include <functional>
#include <numeric>
#include <vector>

using graphene::net::node_id_t;
using graphene::net::rejects_duplicate_hello;

namespace
{
  using duplicate_rule = std::function<bool( const node_id_t& my_node_id, const node_id_t& peer_node_id )>;

  node_id_t make_node_id( char first_byte )
  {
    node_id_t id;
    std::fill( id.data, id.data + id.size(), char( 0x55 ) );
    id.data[0] = first_byte;
    return id;
  }

  /// Two nodes hold `socket_count` connections to each other, opened in either direction, and each
  /// node receives the other's hello once on every one of them.  `arrival_order` lists those
  /// 2 * socket_count arrivals: event e is node (e % 2) receiving the hello on socket (e / 2).
  /// A connection survives only when both nodes accepted the hello on it.  Returns the surviving
  /// sockets.
  std::vector<size_t> surviving_sockets( const node_id_t ids[2], const std::vector<size_t>& arrival_order,
                                         const duplicate_rule& rejects )
  {
    const size_t socket_count = arrival_order.size() / 2;
    std::vector<std::vector<bool>> hello_seen( 2, std::vector<bool>( socket_count, false ) );
    std::vector<std::vector<bool>> accepted( 2, std::vector<bool>( socket_count, false ) );
    for( size_t event : arrival_order )
    {
      const size_t receiver = event % 2;
      const size_t socket = event / 2;
      const bool already_connected = std::find( hello_seen[ receiver ].begin(), hello_seen[ receiver ].end(), true ) !=
                                     hello_seen[ receiver ].end();
      accepted[ receiver ][ socket ] = !( already_connected && rejects( ids[ receiver ], ids[ 1 - receiver ] ) );
      hello_seen[ receiver ][ socket ] = true;
    }
    std::vector<size_t> survivors;
    for( size_t socket = 0; socket < socket_count; ++socket )
      if( accepted[ 0 ][ socket ] && accepted[ 1 ][ socket ] )
        survivors.push_back( socket );
    return survivors;
  }

  /// Number of hello arrival orders, out of all of them, that leave exactly one connection.
  size_t orders_leaving_one_connection( const node_id_t ids[2], size_t socket_count, const duplicate_rule& rejects,
                                        size_t& total_orders )
  {
    std::vector<size_t> order( 2 * socket_count );
    std::iota( order.begin(), order.end(), 0 );
    size_t good = 0;
    total_orders = 0;
    do
    {
      ++total_orders;
      if( surviving_sockets( ids, order, rejects ).size() == 1 )
        ++good;
    } while( std::next_permutation( order.begin(), order.end() ) );
    return good;
  }
}

BOOST_AUTO_TEST_SUITE( p2p_duplicate_connection_tests )

/// The node with the lower id decides; the other one always defers, and our own id is always rejected.
BOOST_AUTO_TEST_CASE( lower_node_id_rejects_duplicates )
{
  const node_id_t low = make_node_id( 0x10 );
  const node_id_t high = make_node_id( 0x70 );

  BOOST_CHECK( rejects_duplicate_hello( low, high ) );
  BOOST_CHECK( !rejects_duplicate_hello( high, low ) );
  BOOST_CHECK( rejects_duplicate_hello( low, low ) );
  BOOST_CHECK( rejects_duplicate_hello( high, high ) );
}

/// Whatever order the hellos arrive in on two or three simultaneous connections between the same
/// pair of nodes, exactly one connection survives on both sides, regardless of which node has the
/// lower id.  Rejecting every duplicate on both sides, as before, can leave none.
BOOST_AUTO_TEST_CASE( crossed_duplicate_hellos_keep_exactly_one_connection )
{
  const node_id_t pairs[2][2] = { { make_node_id( 0x10 ), make_node_id( 0x70 ) },
                                  { make_node_id( 0x70 ), make_node_id( 0x10 ) } };
  const duplicate_rule fixed = []( const node_id_t& mine, const node_id_t& theirs ) {
    return rejects_duplicate_hello( mine, theirs );
  };
  const duplicate_rule symmetric = []( const node_id_t&, const node_id_t& ) { return true; };

  for( const auto& ids : pairs )
    for( size_t socket_count : { 2u, 3u } )
    {
      size_t total = 0;
      BOOST_CHECK_EQUAL( orders_leaving_one_connection( ids, socket_count, fixed, total ), total );
      BOOST_CHECK_LT( orders_leaving_one_connection( ids, socket_count, symmetric, total ), total );
    }
}

/// The connection kept is the one whose hello reached the lower-id node first, as seen in the
/// reported incident: the higher-id node accepted IPv4 first while the lower-id node accepted IPv6 first.
BOOST_AUTO_TEST_CASE( lower_node_id_keeps_its_first_accepted_connection )
{
  const node_id_t ids[2] = { make_node_id( 0x10 ), make_node_id( 0x70 ) };
  const size_t ipv4 = 0, ipv6 = 1;
  // the higher-id node (1) receives the IPv4 hello first; the lower-id node (0) receives the IPv6 one first
  const std::vector<size_t> order = { 2 * ipv4 + 1, 2 * ipv6 + 0, 2 * ipv6 + 1, 2 * ipv4 + 0 };
  const auto survivors = surviving_sockets( ids, order, []( const node_id_t& mine, const node_id_t& theirs ) {
    return rejects_duplicate_hello( mine, theirs );
  } );
  BOOST_REQUIRE_EQUAL( survivors.size(), 1u );
  BOOST_CHECK_EQUAL( survivors.front(), ipv6 );
}

BOOST_AUTO_TEST_SUITE_END()

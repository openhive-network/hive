#include <boost/test/unit_test.hpp>

#include <graphene/net/core_messages.hpp>
#include <graphene/net/node.hpp>
#include <graphene/net/peer_connection.hpp>

#include <chrono>
#include <cstdint>
#include <unordered_set>

using graphene::net::item_hash_t;
using graphene::net::item_id;
using graphene::net::p2p_item_hash;
using graphene::net::peer_connection;
using graphene::net::requested_items;
using graphene::net::transaction_inventory;

namespace {

const size_t colliding_count = 100000;

/// the most items a bucket of a hashed container filled with colliding_count ids may hold: a random hash puts
/// about ten in the fullest one, a hash of the ids' first 8 bytes puts all of them in one
const size_t max_bucket_load = 64;

/// a generous bound on the time to insert and look up colliding_count ids in one container: done linearly that
/// takes well under a second even in a debug build, while scanning one shared bucket takes minutes
const std::chrono::seconds max_container_time( 10 );

const fc::time_point_sec start = fc::time_point_sec( fc::time_point::from_iso_string( "2026-11-21T00:00:00" ) );

/// distinct ids that share their first 8 bytes, which is all std::hash<fc::ripemd160> looks at
item_hash_t colliding_id( uint32_t n )
{
  item_hash_t hash;
  hash._hash[ 0 ] = 0x12345678;
  hash._hash[ 1 ] = 0x9abcdef0;
  hash._hash[ 2 ] = n;
  hash._hash[ 3 ] = ~n;
  hash._hash[ 4 ] = n * 2654435761u;
  return hash;
}

template< typename Container >
size_t fullest_bucket( const Container& container )
{
  size_t fullest = 0;
  for( size_t bucket = 0; bucket < container.bucket_count(); ++bucket )
    fullest = std::max( fullest, container.bucket_size( bucket ) );
  return fullest;
}

template< typename Fill >
void require_linear_time( const char* container_name, Fill&& fill )
{
  const auto began = std::chrono::steady_clock::now();
  fill();
  const auto elapsed = std::chrono::steady_clock::now() - began;
  BOOST_TEST_MESSAGE( container_name << ": " << std::chrono::duration_cast< std::chrono::milliseconds >( elapsed ).count() << " ms" );
  BOOST_CHECK_MESSAGE( elapsed < max_container_time, container_name << " took "
                       << std::chrono::duration_cast< std::chrono::milliseconds >( elapsed ).count() << " ms" );
}

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_item_hash_tests )

/// The hash of an item id depends on the whole id and its type, and is the same for equal ids.
BOOST_AUTO_TEST_CASE( hash_covers_the_whole_id )
{
  const p2p_item_hash hasher;
  BOOST_CHECK_EQUAL( hasher( colliding_id( 1 ) ), hasher( colliding_id( 1 ) ) );
  BOOST_CHECK_NE( hasher( colliding_id( 1 ) ), hasher( colliding_id( 2 ) ) );
  BOOST_CHECK_EQUAL( hasher( item_id( graphene::net::trx_message_type, colliding_id( 1 ) ) ),
                     hasher( item_id( graphene::net::trx_message_type, colliding_id( 1 ) ) ) );
  BOOST_CHECK_NE( hasher( item_id( graphene::net::trx_message_type, colliding_id( 1 ) ) ),
                  hasher( item_id( graphene::net::block_message_type, colliding_id( 1 ) ) ) );
}

/// Ids sharing their first 8 bytes spread over the buckets of the sets fetch_items requests are deduplicated with
/// and of the per-peer maps and indexes of item ids.
BOOST_AUTO_TEST_CASE( colliding_ids_spread_over_buckets )
{
  std::unordered_set< item_hash_t, p2p_item_hash > hashes;
  peer_connection::item_to_time_map_type item_times;
  peer_connection::timestamped_items_set_type timestamped_items;
  require_linear_time( "unordered containers and timestamped item index", [&]()
  {
    for( uint32_t n = 0; n < colliding_count; ++n )
    {
      const item_id item( graphene::net::trx_message_type, colliding_id( n ) );
      BOOST_REQUIRE( hashes.insert( item.item_hash ).second );
      BOOST_REQUIRE( item_times.emplace( item, fc::time_point() ).second );
      BOOST_REQUIRE( timestamped_items.insert( peer_connection::timestamped_item_id( item, start ) ).second );
    }
  } );
  BOOST_CHECK_EQUAL( hashes.size(), colliding_count );
  BOOST_CHECK_LE( fullest_bucket( hashes ), max_bucket_load );
  BOOST_CHECK_LE( fullest_bucket( item_times ), max_bucket_load );
  BOOST_CHECK_LE( fullest_bucket( timestamped_items ), max_bucket_load );
}

/// The transaction inventory records and finds colliding_count advertised ids sharing their first 8 bytes in linear time.
BOOST_AUTO_TEST_CASE( transaction_inventory_with_colliding_ids )
{
  transaction_inventory inventory;
  const auto slot = inventory.acquire_slot();
  require_linear_time( "transaction_inventory", [&]()
  {
    for( uint32_t n = 0; n < colliding_count; ++n )
      BOOST_REQUIRE( inventory.record_advertised_to_us( slot, colliding_id( n ), start ) );
    for( uint32_t n = 0; n < colliding_count; ++n )
      BOOST_REQUIRE( inventory.advertised_to_us( slot, colliding_id( n ) ) );
  } );
  BOOST_CHECK_EQUAL( inventory.size(), colliding_count );
  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( slot ), colliding_count );
}

/// The items requested from a peer hold and find colliding_count ids sharing their first 8 bytes in linear time.
BOOST_AUTO_TEST_CASE( requested_items_with_colliding_ids )
{
  requested_items requested;
  require_linear_time( "requested_items", [&]()
  {
    for( uint32_t n = 0; n < colliding_count; ++n )
      requested.insert( item_id( graphene::net::trx_message_type, colliding_id( n ) ), fc::time_point() );
    for( uint32_t n = 0; n < colliding_count; ++n )
      BOOST_REQUIRE( requested.find( item_id( graphene::net::trx_message_type, colliding_id( n ) ) ) != requested.end() );
  } );
  BOOST_CHECK_EQUAL( requested.size(), colliding_count );
  BOOST_CHECK_EQUAL( requested.transactions(), colliding_count );
}

BOOST_AUTO_TEST_SUITE_END()

#include <boost/test/unit_test.hpp>

#include <graphene/net/core_messages.hpp>
#include <graphene/net/node.hpp>

#include <cstdint>

using graphene::net::item_hash_t;
using graphene::net::transaction_inventory;

namespace {

const fc::time_point_sec start = fc::time_point_sec( fc::time_point::from_iso_string( "2026-11-18T00:00:00" ) );

item_hash_t transaction( uint32_t n )
{
  item_hash_t hash;
  hash._hash[ 0 ] = n;
  hash._hash[ 1 ] = ~n;
  return hash;
}

/// what the node's advertise loop does for one transaction and peer: returns whether it advertises it
bool advertise( transaction_inventory& inventory, transaction_inventory::slot_type slot, const item_hash_t& item, fc::time_point_sec now )
{
  const transaction_inventory::record& item_record = inventory.get( item, now );
  return !item_record.advertised_to_us( slot ) && inventory.record_advertised_to_peer( item_record, slot, now );
}

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_transaction_inventory_tests )

/// A transaction is advertised to a peer once, and never to a peer that advertised it to us; peers are independent.
BOOST_AUTO_TEST_CASE( advertised_once_and_not_to_advertisers )
{
  transaction_inventory inventory;
  const auto advertiser = inventory.acquire_slot();
  const auto other = inventory.acquire_slot();

  BOOST_CHECK( inventory.record_advertised_to_us( advertiser, transaction( 1 ), start ) );
  BOOST_CHECK( !inventory.record_advertised_to_us( advertiser, transaction( 1 ), start ) );
  BOOST_CHECK( inventory.advertised_to_us( advertiser, transaction( 1 ) ) );
  BOOST_CHECK( !inventory.advertised_to_us( other, transaction( 1 ) ) );

  BOOST_CHECK( !advertise( inventory, advertiser, transaction( 1 ), start ) );
  BOOST_CHECK( advertise( inventory, other, transaction( 1 ), start ) );
  BOOST_CHECK( !advertise( inventory, other, transaction( 1 ), start ) );
  BOOST_CHECK( inventory.find( transaction( 1 ) )->advertised_to_peer( other ) );
  BOOST_CHECK( !inventory.find( transaction( 1 ) )->advertised_to_peer( advertiser ) );

  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( advertiser ), 1u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_peer_count( other ), 1u );
  BOOST_CHECK_EQUAL( inventory.size(), 1u );
  BOOST_CHECK( inventory.find( transaction( 2 ) ) == nullptr );
}

/// Once received, a transaction no longer counts against its advertisers' limit, but is still not advertised back to them.
BOOST_AUTO_TEST_CASE( received_transaction_moves_to_advertised_to_peer )
{
  transaction_inventory inventory;
  const auto first = inventory.acquire_slot();
  const auto second = inventory.acquire_slot();
  const auto third = inventory.acquire_slot();
  inventory.record_advertised_to_us( first, transaction( 1 ), start );
  inventory.record_advertised_to_us( second, transaction( 1 ), start );
  inventory.record_advertised_to_us( second, transaction( 2 ), start );

  inventory.mark_received( transaction( 1 ) );
  inventory.mark_received( transaction( 3 ) ); // nobody advertised it

  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( first ), 0u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( second ), 1u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_peer_count( first ), 1u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_peer_count( second ), 1u );
  BOOST_CHECK( !inventory.advertised_to_us( first, transaction( 1 ) ) );
  BOOST_CHECK( !advertise( inventory, first, transaction( 1 ), start ) );
  BOOST_CHECK( !advertise( inventory, second, transaction( 1 ), start ) );
  BOOST_CHECK( advertise( inventory, third, transaction( 1 ), start ) );
  BOOST_CHECK( inventory.find( transaction( 3 ) ) == nullptr );
}

/// A peer that says it doesn't have a transaction stops counting as its source; other advertisers still do.
BOOST_AUTO_TEST_CASE( forgotten_advertisement_leaves_other_advertisers )
{
  transaction_inventory inventory;
  const auto liar = inventory.acquire_slot();
  const auto holder = inventory.acquire_slot();
  inventory.record_advertised_to_us( liar, transaction( 1 ), start );
  inventory.record_advertised_to_us( holder, transaction( 1 ), start );

  inventory.forget_advertised_to_us( liar, transaction( 1 ) );
  inventory.forget_advertised_to_us( liar, transaction( 2 ) ); // no record

  const transaction_inventory::record* item_record = inventory.find( transaction( 1 ) );
  BOOST_REQUIRE( item_record != nullptr );
  BOOST_CHECK( !item_record->advertised_to_us( liar ) );
  BOOST_CHECK( item_record->advertised_to_us( holder ) );
  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( liar ), 0u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( holder ), 1u );
  // it may advertise it again
  BOOST_CHECK( inventory.record_advertised_to_us( liar, transaction( 1 ), start ) );
}

/// Records expire a window after the last peer was recorded in them, in one pass, and their counts go with them.
BOOST_AUTO_TEST_CASE( records_expire_after_the_last_peer_was_recorded )
{
  transaction_inventory inventory;
  const auto first = inventory.acquire_slot();
  const auto second = inventory.acquire_slot();
  inventory.record_advertised_to_us( first, transaction( 1 ), start );
  inventory.record_advertised_to_us( first, transaction( 2 ), start );
  inventory.record_advertised_to_us( first, transaction( 3 ), start + 2 );
  // knowledge moved by receiving it doesn't make 3 newer
  inventory.mark_received( transaction( 3 ) );
  BOOST_CHECK( advertise( inventory, second, transaction( 2 ), start + 5 ) ); // keeps 2 for longer

  inventory.expire( start );
  BOOST_CHECK_EQUAL( inventory.size(), 3u );

  inventory.expire( start + 1 );
  BOOST_CHECK( inventory.find( transaction( 1 ) ) == nullptr );
  BOOST_CHECK_EQUAL( inventory.size(), 2u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( first ), 1u );

  inventory.expire( start + 3 );
  BOOST_CHECK( inventory.find( transaction( 3 ) ) == nullptr );
  BOOST_CHECK( inventory.advertised_to_us( first, transaction( 2 ) ) );
  BOOST_CHECK_EQUAL( inventory.advertised_to_peer_count( first ), 0u );

  inventory.expire( start + 6 );
  BOOST_CHECK_EQUAL( inventory.size(), 0u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( first ), 0u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_peer_count( second ), 0u );

  // after expiry the transaction may be advertised again
  BOOST_CHECK( inventory.record_advertised_to_us( first, transaction( 1 ), start + 70 ) );
}

/// A record kept while its transaction is outstanding with a peer lives a window from then, whoever advertised it.
BOOST_AUTO_TEST_CASE( kept_record_expires_a_window_after_it_was_kept )
{
  transaction_inventory inventory;
  const auto advertiser = inventory.acquire_slot();
  inventory.record_advertised_to_us( advertiser, transaction( 1 ), start );
  inventory.record_advertised_to_us( advertiser, transaction( 2 ), start );
  inventory.keep( transaction( 1 ), start + 3 );
  inventory.keep( transaction( 1 ), start + 2 ); // never makes it older
  inventory.keep( transaction( 3 ), start + 3 ); // nor creates one
  BOOST_CHECK( inventory.find( transaction( 3 ) ) == nullptr );

  inventory.expire( start + 1 );
  BOOST_CHECK( inventory.find( transaction( 2 ) ) == nullptr );
  BOOST_CHECK( inventory.advertised_to_us( advertiser, transaction( 1 ) ) );
  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( advertiser ), 1u );

  inventory.expire( start + 3 );
  BOOST_CHECK( inventory.advertised_to_us( advertiser, transaction( 1 ) ) );
  inventory.expire( start + 4 );
  BOOST_CHECK_EQUAL( inventory.size(), 0u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( advertiser ), 0u );
}

/// A released slot is cleared from every record, so the next peer given the slot knows nothing.
BOOST_AUTO_TEST_CASE( reused_slot_inherits_no_knowledge )
{
  transaction_inventory inventory;
  const auto staying = inventory.acquire_slot();
  const auto leaving = inventory.acquire_slot();
  inventory.record_advertised_to_us( leaving, transaction( 1 ), start );
  BOOST_CHECK( advertise( inventory, leaving, transaction( 2 ), start ) );
  inventory.record_advertised_to_us( staying, transaction( 1 ), start );

  inventory.release_slot( leaving );
  const auto newcomer = inventory.acquire_slot();
  BOOST_CHECK_EQUAL( newcomer, leaving );

  BOOST_CHECK( !inventory.advertised_to_us( newcomer, transaction( 1 ) ) );
  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( newcomer ), 0u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_peer_count( newcomer ), 0u );
  BOOST_CHECK( advertise( inventory, newcomer, transaction( 2 ), start ) );
  BOOST_CHECK( inventory.advertised_to_us( staying, transaction( 1 ) ) );
  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( staying ), 1u );

  BOOST_CHECK_THROW( inventory.release_slot( 7 ), fc::assert_exception );
}

/// Slots past the first word's 64 work like the others, in records created before and after they were taken.
BOOST_AUTO_TEST_CASE( slots_past_one_word )
{
  transaction_inventory inventory;
  inventory.record_advertised_to_us( inventory.acquire_slot(), transaction( 1 ), start );
  transaction_inventory::slot_type last = 0;
  for( int i = 1; i < 200; ++i )
    last = inventory.acquire_slot();
  BOOST_REQUIRE_EQUAL( last, 199u );

  BOOST_CHECK( !inventory.advertised_to_us( last, transaction( 1 ) ) );
  BOOST_CHECK( advertise( inventory, last, transaction( 1 ), start ) );
  BOOST_CHECK( inventory.record_advertised_to_us( 130, transaction( 1 ), start ) );
  BOOST_CHECK( inventory.advertised_to_us( 130, transaction( 1 ) ) );
  BOOST_CHECK( !inventory.advertised_to_us( 66, transaction( 1 ) ) );
  BOOST_CHECK( inventory.record_advertised_to_us( 64, transaction( 2 ), start ) );

  inventory.mark_received( transaction( 1 ) );
  BOOST_CHECK( inventory.find( transaction( 1 ) )->advertised_to_peer( 130 ) );
  BOOST_CHECK( inventory.find( transaction( 1 ) )->advertised_to_peer( 0 ) );
  BOOST_CHECK_EQUAL( inventory.advertised_to_peer_count( 130 ), 1u );

  inventory.release_slot( 130 );
  BOOST_CHECK_EQUAL( inventory.acquire_slot(), 130u );
  BOOST_CHECK( !inventory.find( transaction( 1 ) )->advertised_to_peer( 130 ) );

  inventory.expire( start + 1 );
  BOOST_CHECK_EQUAL( inventory.size(), 0u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_us_count( 64 ), 0u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_peer_count( last ), 0u );
  BOOST_CHECK_EQUAL( inventory.advertised_to_peer_count( 0 ), 0u );
}

BOOST_AUTO_TEST_SUITE_END()

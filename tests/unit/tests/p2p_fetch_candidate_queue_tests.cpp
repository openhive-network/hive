#include <boost/test/unit_test.hpp>

#include <graphene/net/core_messages.hpp>
#include <graphene/net/node.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <vector>

using graphene::net::block_message_type;
using graphene::net::fetch_candidate_queue;
using graphene::net::fetch_candidate_verdict;
using graphene::net::item_hash_t;
using graphene::net::item_id;
using graphene::net::trx_message_type;

namespace {

const fc::time_point start = fc::time_point::from_iso_string( "2026-11-17T00:00:00" );
const fc::time_point keep_all = fc::time_point();

item_id make_item( uint32_t type, uint32_t n )
{
  item_hash_t hash;
  hash._hash[ 0 ] = n;
  return item_id( type, hash );
}

item_id block( uint32_t n ) { return make_item( block_message_type, n ); }
item_id transaction( uint32_t n ) { return make_item( trx_message_type, n ); }

/// Judges every entry with @p verdict_for and returns the items taken in one pass
std::vector<item_id> take_all( fetch_candidate_queue& queue, bool include_transactions,
                               const std::function<fetch_candidate_verdict( const item_id& )>& verdict_for, uint64_t& examined )
{
  std::vector<item_id> taken;
  queue.begin_pass();
  while( std::optional<item_id> item = queue.take_next( include_transactions, verdict_for, examined ) )
    taken.push_back( *item );
  return taken;
}

fetch_candidate_verdict take_any( const item_id& ) { return fetch_candidate_verdict::take; }

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_fetch_candidate_queue_tests )

/// A block queued after transactions is still requested first; transactions follow in the order they were queued.
BOOST_AUTO_TEST_CASE( blocks_come_before_transactions )
{
  fetch_candidate_queue queue;
  queue.push( transaction( 1 ), start, keep_all );
  queue.push( transaction( 2 ), start, keep_all );
  queue.push( block( 3 ), start, keep_all );
  queue.push( transaction( 4 ), start, keep_all );

  uint64_t examined = 0;
  const std::vector<item_id> expected{ block( 3 ), transaction( 1 ), transaction( 2 ), transaction( 4 ) };
  BOOST_CHECK( take_all( queue, true, take_any, examined ) == expected );
  BOOST_CHECK_EQUAL( examined, 4u );
  BOOST_CHECK_EQUAL( queue.size(), 0u );
}

/// Entries for items no longer wanted are skipped and removed, at the cost of one look each.
BOOST_AUTO_TEST_CASE( dropped_entries_are_never_taken )
{
  fetch_candidate_queue queue;
  for( uint32_t n = 1; n <= 6; ++n )
    queue.push( transaction( n ), start, keep_all );
  const std::set<uint32_t> no_longer_wanted{ 1, 2, 5 };

  uint64_t examined = 0;
  const auto judge = [&]( const item_id& item ) {
    return no_longer_wanted.count( item.item_hash._hash[ 0 ] ) ? fetch_candidate_verdict::drop : fetch_candidate_verdict::take;
  };
  const std::vector<item_id> expected{ transaction( 3 ), transaction( 4 ), transaction( 6 ) };
  BOOST_CHECK( take_all( queue, true, judge, examined ) == expected );
  BOOST_CHECK_EQUAL( examined, 6u );
  BOOST_CHECK_EQUAL( queue.size(), 0u );

  BOOST_CHECK( take_all( queue, true, take_any, examined ).empty() );
  BOOST_CHECK_EQUAL( examined, 6u );
}

/// A kept entry (a block a deprioritized peer must leave to another source) is not looked at again in the same pass,
/// does not hold back the entries after it, and is offered again in the next pass.
BOOST_AUTO_TEST_CASE( kept_entries_wait_for_the_next_pass )
{
  fetch_candidate_queue queue;
  queue.push( block( 1 ), start, keep_all );
  queue.push( block( 2 ), start, keep_all );
  queue.push( transaction( 3 ), start, keep_all );

  uint64_t examined = 0;
  const auto keep_block_1 = [&]( const item_id& item ) {
    return item == block( 1 ) ? fetch_candidate_verdict::keep : fetch_candidate_verdict::take;
  };
  const std::vector<item_id> first_pass{ block( 2 ), transaction( 3 ) };
  BOOST_CHECK( take_all( queue, true, keep_block_1, examined ) == first_pass );
  BOOST_CHECK_EQUAL( examined, 3u );
  BOOST_CHECK_EQUAL( queue.size(), 1u );

  const std::vector<item_id> second_pass{ block( 1 ) };
  BOOST_CHECK( take_all( queue, true, take_any, examined ) == second_pass );
  BOOST_CHECK_EQUAL( queue.size(), 0u );
}

/// While transaction fetching from the peer is inhibited, its blocks are still taken and its transactions are left
/// untouched, unexamined, for later.
BOOST_AUTO_TEST_CASE( transactions_wait_while_excluded )
{
  fetch_candidate_queue queue;
  queue.push( transaction( 1 ), start, keep_all );
  queue.push( block( 2 ), start, keep_all );

  uint64_t examined = 0;
  const std::vector<item_id> blocks_only{ block( 2 ) };
  BOOST_CHECK( take_all( queue, false, take_any, examined ) == blocks_only );
  BOOST_CHECK_EQUAL( examined, 1u );
  BOOST_CHECK( queue.has_transactions() );

  const std::vector<item_id> later{ transaction( 1 ) };
  BOOST_CHECK( take_all( queue, true, take_any, examined ) == later );
  BOOST_CHECK( !queue.has_transactions() );
}

/// Adding an entry discards the ones queued before the fetch window, kept ones included, so a peer that is never idle
/// holds no more entries than it advertised within the window.
BOOST_AUTO_TEST_CASE( entries_older_than_the_window_are_trimmed )
{
  fetch_candidate_queue queue;
  queue.push( block( 1 ), start, keep_all );
  queue.push( transaction( 2 ), start, keep_all );
  queue.push( transaction( 3 ), start + fc::seconds( 30 ), keep_all );

  uint64_t examined = 0;
  const auto keep_everything = []( const item_id& ) { return fetch_candidate_verdict::keep; };
  BOOST_CHECK( take_all( queue, true, keep_everything, examined ).empty() );

  queue.push( transaction( 4 ), start + fc::seconds( 61 ), start + fc::seconds( 1 ) );
  BOOST_CHECK_EQUAL( queue.size(), 2u );

  const std::vector<item_id> remaining{ transaction( 3 ), transaction( 4 ) };
  BOOST_CHECK( take_all( queue, true, take_any, examined ) == remaining );
}

/// A disconnected peer's queue is discarded.
BOOST_AUTO_TEST_CASE( clear_discards_every_entry )
{
  fetch_candidate_queue queue;
  queue.push( block( 1 ), start, keep_all );
  queue.push( transaction( 2 ), start, keep_all );
  queue.clear();
  BOOST_CHECK_EQUAL( queue.size(), 0u );
  BOOST_CHECK( !queue.has_transactions() );

  uint64_t examined = 0;
  BOOST_CHECK( take_all( queue, true, take_any, examined ).empty() );
  BOOST_CHECK_EQUAL( examined, 0u );
}

/// A queued block is found among the entries until it is taken; transactions are not looked through.
BOOST_AUTO_TEST_CASE( has_block_finds_queued_blocks_only )
{
  fetch_candidate_queue queue;
  queue.push( block( 1 ), start, keep_all );
  queue.push( transaction( 2 ), start, keep_all );
  queue.push( block( 3 ), start, keep_all );
  BOOST_CHECK( queue.has_block( block( 1 ) ) );
  BOOST_CHECK( queue.has_block( block( 3 ) ) );
  BOOST_CHECK( !queue.has_block( block( 2 ) ) );
  BOOST_CHECK( !queue.has_block( transaction( 2 ) ) );

  uint64_t examined = 0;
  queue.begin_pass();
  BOOST_CHECK( queue.take_next( false, take_any, examined ) == std::optional<item_id>( block( 1 ) ) );
  BOOST_CHECK( !queue.has_block( block( 1 ) ) );
  BOOST_CHECK( queue.has_block( block( 3 ) ) );
}

BOOST_AUTO_TEST_SUITE_END()

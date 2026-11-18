#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/message_cache.hpp>

#include <hive/chain/full_block.hpp>
#include <hive/chain/full_transaction.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <fc/exception/exception.hpp>

#include <memory>
#include <string>
#include <vector>

using graphene::net::blockchain_tied_message_cache;
using graphene::net::item_hash_t;
using graphene::net::message_propagation_data;
using hive::chain::full_block_type;
using hive::chain::full_transaction_type;

namespace {

typedef blockchain_tied_message_cache<std::shared_ptr<full_transaction_type>> transaction_cache;
typedef blockchain_tied_message_cache<std::shared_ptr<full_block_type>> block_cache;

const uint32_t cache_duration = GRAPHENE_NET_MESSAGE_CACHE_DURATION_IN_BLOCKS;

/// A transaction whose id depends on @p n and whose message hash also depends on @p signature_byte.
std::shared_ptr<full_transaction_type> make_transaction( uint32_t n, unsigned char signature_byte = 1 )
{
  hive::protocol::custom_json_operation op;
  op.required_posting_auths.insert( "alice" );
  op.id = "message_cache_test";
  op.json = std::to_string( n );
  hive::protocol::signed_transaction tx;
  tx.operations.push_back( op );
  hive::protocol::signature_type signature;
  for( auto& byte : signature.data )
    byte = signature_byte;
  tx.signatures.push_back( signature );
  return full_transaction_type::create_from_signed_transaction( tx, hive::protocol::pack_type::hf26, false );
}

std::shared_ptr<full_block_type> make_block( uint32_t n )
{
  hive::protocol::signed_block block;
  block.witness = "initminer";
  block.timestamp = fc::time_point_sec( n );
  return full_block_type::create_from_signed_block( block );
}

message_propagation_data propagation( int64_t received_us )
{
  message_propagation_data data;
  data.received_time = fc::time_point( fc::microseconds( received_us ) );
  data.validated_time = data.received_time;
  return data;
}

void accept_blocks( transaction_cache& cache, uint32_t count )
{
  for( uint32_t i = 0; i < count; ++i )
    cache.block_accepted();
}

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_message_cache_tests )

/// A cached transaction is found by its message hash and by its id; others are not.
BOOST_AUTO_TEST_CASE( transaction_lookup_by_message_hash_and_id )
{
  transaction_cache cache( 1024 );
  const auto first = make_transaction( 1 );
  const auto second = make_transaction( 2 );
  const auto absent = make_transaction( 3 );
  cache.cache_message( first, propagation( 10 ) );
  cache.cache_message( second, propagation( 20 ) );

  BOOST_CHECK_EQUAL( cache.size(), 2u );
  BOOST_CHECK( cache.get_item( first->get_legacy_transaction_message_hash() ) == first );
  BOOST_CHECK( cache.get_item( second->get_legacy_transaction_message_hash() ) == second );
  BOOST_CHECK( cache.get_item_by_contents_hash( second->get_transaction_id() ) == second );
  BOOST_CHECK( cache.contains_item( first->get_legacy_transaction_message_hash() ) );
  BOOST_CHECK( cache.contains_item_by_contents_hash( first->get_transaction_id() ) );
  BOOST_CHECK( cache.get_message_propagation_data( second->get_transaction_id() ).received_time == propagation( 20 ).received_time );

  BOOST_CHECK( !cache.contains_item( absent->get_legacy_transaction_message_hash() ) );
  BOOST_CHECK( !cache.contains_item_by_contents_hash( absent->get_transaction_id() ) );
  // a message hash is not a contents hash and vice versa
  BOOST_CHECK( !cache.contains_item( first->get_transaction_id() ) );
  BOOST_CHECK( !cache.contains_item_by_contents_hash( first->get_legacy_transaction_message_hash() ) );
  BOOST_CHECK_THROW( cache.get_item( absent->get_legacy_transaction_message_hash() ), fc::key_not_found_exception );
  BOOST_CHECK_THROW( cache.get_item_by_contents_hash( absent->get_transaction_id() ), fc::key_not_found_exception );
  BOOST_CHECK_THROW( cache.get_message_propagation_data( absent->get_transaction_id() ), fc::key_not_found_exception );
  BOOST_CHECK_THROW( cache.get_message_propagation_data( item_hash_t() ), fc::key_not_found_exception );
}

/// Caching the same message again keeps the first copy and its propagation data.
BOOST_AUTO_TEST_CASE( same_message_is_cached_once )
{
  transaction_cache cache;
  const auto tx = make_transaction( 1 );
  cache.cache_message( tx, propagation( 10 ) );
  cache.cache_message( make_transaction( 1 ), propagation( 20 ) );

  BOOST_CHECK_EQUAL( cache.size(), 1u );
  BOOST_CHECK( cache.get_item( tx->get_legacy_transaction_message_hash() ) == tx );
  BOOST_CHECK( cache.get_message_propagation_data( tx->get_transaction_id() ).received_time == propagation( 10 ).received_time );
}

/// Two messages of one transaction id (differently signed) are both kept; the id resolves to the first one.
BOOST_AUTO_TEST_CASE( shared_transaction_id_resolves_to_first_cached )
{
  transaction_cache cache;
  const auto first = make_transaction( 1, 1 );
  const auto resigned = make_transaction( 1, 2 );
  BOOST_REQUIRE( first->get_transaction_id() == resigned->get_transaction_id() );
  BOOST_REQUIRE( first->get_legacy_transaction_message_hash() != resigned->get_legacy_transaction_message_hash() );
  cache.cache_message( first, propagation( 10 ) );
  cache.cache_message( resigned, propagation( 20 ) );

  BOOST_CHECK_EQUAL( cache.size(), 2u );
  BOOST_CHECK( cache.get_item( resigned->get_legacy_transaction_message_hash() ) == resigned );
  BOOST_CHECK( cache.get_item_by_contents_hash( first->get_transaction_id() ) == first );
  BOOST_CHECK( cache.get_message_propagation_data( first->get_transaction_id() ).received_time == propagation( 10 ).received_time );
}

/// An item stays served for GRAPHENE_NET_MESSAGE_CACHE_DURATION_IN_BLOCKS accepted blocks and is dropped with the next one.
BOOST_AUTO_TEST_CASE( items_expire_after_cache_duration )
{
  transaction_cache cache;
  const auto old_tx = make_transaction( 1 );
  const auto newer_tx = make_transaction( 2 );
  cache.cache_message( old_tx, propagation( 10 ) );
  accept_blocks( cache, 1 );
  cache.cache_message( newer_tx, propagation( 20 ) );

  accept_blocks( cache, cache_duration - 1 );
  BOOST_CHECK( cache.contains_item( old_tx->get_legacy_transaction_message_hash() ) );
  BOOST_CHECK_EQUAL( cache.size(), 2u );

  accept_blocks( cache, 1 );
  BOOST_CHECK( !cache.contains_item( old_tx->get_legacy_transaction_message_hash() ) );
  BOOST_CHECK( !cache.contains_item_by_contents_hash( old_tx->get_transaction_id() ) );
  BOOST_CHECK_THROW( cache.get_item( old_tx->get_legacy_transaction_message_hash() ), fc::key_not_found_exception );
  BOOST_CHECK( cache.get_item( newer_tx->get_legacy_transaction_message_hash() ) == newer_tx );
  BOOST_CHECK_EQUAL( cache.size(), 1u );

  accept_blocks( cache, 1 );
  BOOST_CHECK( !cache.contains_item_by_contents_hash( newer_tx->get_transaction_id() ) );
  BOOST_CHECK_EQUAL( cache.size(), 0u );
}

/// Many cached items are all found again, also past the reserved size.
BOOST_AUTO_TEST_CASE( every_cached_transaction_is_found )
{
  transaction_cache cache( 16 );
  std::vector<std::shared_ptr<full_transaction_type>> transactions;
  for( uint32_t n = 0; n < 500; ++n )
  {
    transactions.push_back( make_transaction( n ) );
    cache.cache_message( transactions.back(), propagation( n ) );
  }
  BOOST_CHECK_EQUAL( cache.size(), transactions.size() );
  for( const auto& tx : transactions )
  {
    BOOST_CHECK( cache.get_item( tx->get_legacy_transaction_message_hash() ) == tx );
    BOOST_CHECK( cache.get_item_by_contents_hash( tx->get_transaction_id() ) == tx );
  }
}

/// Blocks are found by legacy message hash and by block id.
BOOST_AUTO_TEST_CASE( block_lookup_by_message_hash_and_id )
{
  block_cache cache;
  const auto block = make_block( 1 );
  const auto absent = make_block( 2 );
  cache.cache_message( block, propagation( 10 ) );

  BOOST_CHECK( cache.get_item( block->get_legacy_block_message_hash() ) == block );
  BOOST_CHECK( cache.get_item_by_contents_hash( block->get_block_id() ) == block );
  BOOST_CHECK( !cache.contains_item( absent->get_legacy_block_message_hash() ) );
  BOOST_CHECK_THROW( cache.get_item_by_contents_hash( absent->get_block_id() ), fc::key_not_found_exception );
}

BOOST_AUTO_TEST_SUITE_END()

#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/witness_fixture.hpp"
#include "../db_fixture/p2p_test_peer.hpp"

#include <hive/plugins/p2p/p2p_plugin.hpp>

#include <graphene/net/config.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

using p2p_tests::test_peer;

/// The node listens on loopback only, for the peers the test runs in-process
const std::string loopback_p2p_parameters = R"({"listen_endpoint":"127.0.0.1:0","accept_incoming_connections":true})";

bool wait_until( const std::function< bool() >& condition, fc::microseconds timeout )
{
  const fc::time_point deadline = fc::time_point::now() + timeout;
  while( !condition() )
  {
    if( fc::time_point::now() >= deadline )
      return false;
    fc::usleep( fc::milliseconds( 50 ) );
  }
  return true;
}

bool all_advertised( const test_peer& peer, const std::vector< full_transaction_ptr >& transactions )
{
  return std::all_of( transactions.begin(), transactions.end(), [&]( const full_transaction_ptr& transaction ) {
    return peer.was_advertised( transaction->get_legacy_transaction_message_hash() );
  } );
}

std::vector< graphene::net::item_hash_t > hashes_of( const std::vector< full_transaction_ptr >& transactions )
{
  std::vector< graphene::net::item_hash_t > hashes;
  for( const full_transaction_ptr& transaction : transactions )
    hashes.push_back( transaction->get_legacy_transaction_message_hash() );
  return hashes;
}

struct p2p_transaction_submission_fixture : public witness_fixture
{
  fc::ip::endpoint node_endpoint()
  {
    return theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_info()[ "listening_on" ].as< fc::ip::endpoint >();
  }

  /// Connects @p peer and waits until the node advertises transactions to it: only to peers it has no blocks to offer
  void connect_in_sync( test_peer& peer )
  {
    peer.connect( node_endpoint() );
    BOOST_REQUIRE( wait_until( [&]() { return peer.in_sync(); }, fc::seconds( 10 ) ) );
    BOOST_REQUIRE( wait_until( [&]()
    {
      if( peer.node_considers_us_in_sync() )
        return true;
      block_id_type head;
      db->with_read_lock( [&]() { head = db->head_block_id(); } );
      peer.tell_head( head );
      fc::usleep( fc::milliseconds( 200 ) );
      return peer.node_considers_us_in_sync();
    }, fc::seconds( 10 ) ) );
  }

  /// Starts a node with @p extra_config_args, connects a sending and an observing peer to it and runs @p test
  /// with them on an API thread.
  void run( const config_arg_override_t& extra_config_args,
            const std::function< void( fc::thread& peer_thread, test_peer& sender, test_peer& observer ) >& test )
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    config_arg_override_t config_args = { config_line_t( { "p2p-parameters", { loopback_p2p_parameters } } ) };
    config_args.insert( config_args.end(), extra_config_args.begin(), extra_config_args.end() );
    initialize( 3, {}, { "initminer" }, config_args );
    bool test_passed = false;
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        fc::sleep_until( get_genesis_time() + 2 * HIVE_BLOCK_INTERVAL );
        BOOST_REQUIRE_GT( get_block_num(), 0u );

        fc::thread peer_thread( "test_peers" );
        test_peer sender( peer_thread, db->get_chain_id(), "sender" );
        test_peer observer( peer_thread, db->get_chain_id(), "observer" );
        sender.connect( node_endpoint() );
        BOOST_REQUIRE( wait_until( [&]() { return sender.in_sync(); }, fc::seconds( 10 ) ) );
        connect_in_sync( observer );

        test( peer_thread, sender, observer );

        BOOST_CHECK( !sender.closed() );
        BOOST_CHECK( !observer.closed() );
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
  }
};

} // namespace

// Part of witness_tests so that, like the other tests on a real-time witness node, each runs as a unit of its own.
BOOST_FIXTURE_TEST_SUITE( witness_tests, p2p_transaction_submission_fixture )

/// One peer sends two dependent transactions back to back and one the chain rejects. The dependent ones are
/// both accepted and relayed to the other peer; the rejected one is not relayed, and not fetched again when
/// the other peer advertises it. Transactions a peer sent right before it disconnected are still relayed.
BOOST_AUTO_TEST_CASE( submitted_transactions_are_relayed_once_validated )
{
  try
  {
    run( {}, [&]( fc::thread& peer_thread, test_peer& sender, test_peer& observer )
    {
      schedule_account_create( "alice" );
      schedule_vest( "alice", ASSET( "1000.000 TESTS" ) ); // RC to transact with

      // alice starts without liquid HIVE, so spending is valid only after funding
      const full_transaction_ptr fund = build_transfer( HIVE_INIT_MINER_NAME, "alice", ASSET( "1.000 TESTS" ), "fund", get_default_expiration() );
      const full_transaction_ptr spend = build_transfer( "alice", HIVE_NULL_ACCOUNT, ASSET( "1.000 TESTS" ), "spend", get_default_expiration() );
      const full_transaction_ptr overspend = build_transfer( "alice", HIVE_NULL_ACCOUNT, ASSET( "1000.000 TESTS" ), "overspend", get_default_expiration() );
      const std::vector< full_transaction_ptr > sent = { fund, spend, overspend };
      sender.provide( sent );
      sender.advertise( hashes_of( sent ) );

      BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return all_advertised( observer, { fund, spend } ); }, fc::seconds( 10 ) ),
                             "the dependent transactions were not both accepted and relayed" );
      BOOST_CHECK( sender.was_requested( overspend->get_legacy_transaction_message_hash() ) );
      // the rejected transaction is validated after the ones before it, so it has been for a while by now
      fc::usleep( fc::seconds( 1 ) );
      BOOST_CHECK( !observer.was_advertised( overspend->get_legacy_transaction_message_hash() ) );

      observer.provide( { overspend } );
      observer.advertise( { overspend->get_legacy_transaction_message_hash() } );
      fc::usleep( fc::seconds( 1 ) );
      BOOST_CHECK( !observer.was_requested( overspend->get_legacy_transaction_message_hash() ) );

      test_peer leaving( peer_thread, db->get_chain_id(), "leaving" );
      leaving.connect( node_endpoint() );
      BOOST_REQUIRE( wait_until( [&]() { return leaving.in_sync(); }, fc::seconds( 10 ) ) );
      std::vector< full_transaction_ptr > left_behind;
      for( int i = 0; i < 50; ++i )
        left_behind.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                               "left behind " + std::to_string( i ), get_default_expiration() ) );
      leaving.disconnect_after_delivery();
      leaving.provide( left_behind );
      leaving.advertise( hashes_of( left_behind ) );
      BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return leaving.closed(); }, fc::seconds( 10 ) ),
                             "the leaving peer was not asked for its transactions" );
      // a new peer is first asked for only as many transactions as fit the request byte limit at the largest
      // transaction size, so it delivers just those before it leaves
      std::vector< full_transaction_ptr > delivered;
      std::copy_if( left_behind.begin(), left_behind.end(), std::back_inserter( delivered ), [&]( const full_transaction_ptr& transaction ) {
        return leaving.was_requested( transaction->get_legacy_transaction_message_hash() );
      } );
      BOOST_REQUIRE( !delivered.empty() );
      BOOST_CHECK_MESSAGE( wait_until( [&]() { return all_advertised( observer, delivered ); }, fc::seconds( 10 ) ),
                           "transactions of a peer that disconnected were not relayed" );
    } );
  }
  FC_LOG_AND_RETHROW()
}

/// With room for only two transactions in flight, a peer flooding the node gets every transaction accepted
/// and relayed, and stays connected.
BOOST_AUTO_TEST_CASE( flood_beyond_the_in_flight_limit_is_accepted )
{
  try
  {
    run( { config_line_t( { "p2p-max-transactions-in-flight", { "2" } } ) },
         [&]( fc::thread&, test_peer& sender, test_peer& observer )
    {
      BOOST_REQUIRE_EQUAL( theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_advanced_node_parameters().maximum_transactions_in_flight, 2u );

      std::vector< full_transaction_ptr > flood;
      for( int i = 0; i < 500; ++i )
        flood.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                         "flood " + std::to_string( i ), get_default_expiration() ) );
      sender.provide( flood );
      sender.advertise( hashes_of( flood ) );
      BOOST_CHECK_MESSAGE( wait_until( [&]() { return all_advertised( observer, flood ); }, fc::seconds( 30 ) ),
                           "not every transaction of the flood was accepted and relayed" );
    } );
  }
  FC_LOG_AND_RETHROW()
}

/// A transaction requested from a peer that turns out not to have it, or that disconnects, is fetched from
/// another peer that advertised it meanwhile. A peer connecting after another one left knows nothing of what
/// the one that left advertised: the node advertises it such a transaction once it has it.
BOOST_AUTO_TEST_CASE( transactions_are_refetched_from_other_advertisers )
{
  try
  {
    run( {}, [&]( fc::thread& peer_thread, test_peer& sender, test_peer& observer )
    {
      std::vector< full_transaction_ptr > transactions;
      for( int i = 0; i < 3; ++i )
        transactions.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                "refetched " + std::to_string( i ), get_default_expiration() ) );
      const full_transaction_ptr not_available = transactions[ 0 ];
      const full_transaction_ptr disconnected = transactions[ 1 ];
      const full_transaction_ptr left_behind = transactions[ 2 ];
      const auto hash = []( const full_transaction_ptr& transaction ) { return transaction->get_legacy_transaction_message_hash(); };

      // each source advertises its transaction first and withholds it, so the node asks it rather than the sender
      const auto request_from_withholding_peer = [&]( test_peer& source, const full_transaction_ptr& transaction )
      {
        source.connect( node_endpoint() );
        BOOST_REQUIRE( wait_until( [&]() { return source.in_sync(); }, fc::seconds( 10 ) ) );
        source.advertise( { hash( transaction ) } );
        BOOST_REQUIRE( wait_until( [&]() { return source.was_requested( hash( transaction ) ); }, fc::seconds( 10 ) ) );
        sender.provide( { transaction } );
        sender.advertise( { hash( transaction ) } );
        fc::usleep( fc::milliseconds( 500 ) );
        BOOST_CHECK( !sender.was_requested( hash( transaction ) ) );
      };

      {
        test_peer liar( peer_thread, db->get_chain_id(), "liar" );
        request_from_withholding_peer( liar, not_available );
        liar.stop_withholding();
        BOOST_CHECK_MESSAGE( wait_until( [&]() { return all_advertised( observer, { not_available } ); }, fc::seconds( 10 ) ),
                             "a transaction a peer didn't have was not fetched from another peer that advertised it" );
        BOOST_CHECK( sender.was_requested( hash( not_available ) ) );
      }

      {
        auto leaving = std::make_unique< test_peer >( peer_thread, db->get_chain_id(), "leaving" );
        request_from_withholding_peer( *leaving, disconnected );
        leaving.reset();
        BOOST_CHECK_MESSAGE( wait_until( [&]() { return all_advertised( observer, { disconnected } ); }, fc::seconds( 10 ) ),
                             "a transaction requested from a peer that disconnected was not fetched from another peer that advertised it" );
        BOOST_CHECK( sender.was_requested( hash( disconnected ) ) );
      }

      {
        test_peer withholding( peer_thread, db->get_chain_id(), "withholding" );
        withholding.connect( node_endpoint() );
        BOOST_REQUIRE( wait_until( [&]() { return withholding.in_sync(); }, fc::seconds( 10 ) ) );
        withholding.advertise( { hash( left_behind ) } );
        BOOST_REQUIRE( wait_until( [&]() { return withholding.was_requested( hash( left_behind ) ); }, fc::seconds( 10 ) ) );
      }
      // the newcomer takes the place the withholding peer left in the node's inventory
      BOOST_REQUIRE( wait_until( [&]() { return theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_connected_peers().size() == 2; },
                                 fc::seconds( 10 ) ) );
      test_peer newcomer( peer_thread, db->get_chain_id(), "newcomer" );
      connect_in_sync( newcomer );
      sender.provide( { left_behind } );
      sender.advertise( { hash( left_behind ) } );
      BOOST_CHECK_MESSAGE( wait_until( [&]() { return all_advertised( observer, { left_behind } ); }, fc::seconds( 10 ) ),
                           "a transaction a peer that left had advertised was not fetched from another peer" );
      BOOST_CHECK_MESSAGE( wait_until( [&]() { return newcomer.was_advertised( hash( left_behind ) ); }, fc::seconds( 5 ) ),
                           "a peer that connected after another one left counted as knowing what that one advertised" );
      BOOST_CHECK( !newcomer.closed() );
    } );
  }
  FC_LOG_AND_RETHROW()
}

/// A peer sending small transactions is asked for more of them before it has delivered the earlier ones,
/// past what one request may ask for. When it leaves with all of them outstanding, every one is fetched
/// from another peer that advertised it meanwhile.
BOOST_AUTO_TEST_CASE( pipelined_requests_are_refetched_when_the_peer_leaves )
{
  try
  {
    run( {}, [&]( fc::thread& peer_thread, test_peer& sender, test_peer& observer )
    {
      const auto transfers = [&]( const std::string& memo, int count )
      {
        std::vector< full_transaction_ptr > transactions;
        for( int i = 0; i < count; ++i )
          transactions.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                  memo + " " + std::to_string( i ), get_default_expiration() ) );
        return transactions;
      };
      const auto all_requested = [&]( const test_peer& peer, const std::vector< full_transaction_ptr >& transactions )
      {
        return std::all_of( transactions.begin(), transactions.end(), [&]( const full_transaction_ptr& transaction ) {
          return peer.was_requested( transaction->get_legacy_transaction_message_hash() );
        } );
      };
      const auto none_requested = [&]( const test_peer& peer, const std::vector< full_transaction_ptr >& transactions )
      {
        return std::none_of( transactions.begin(), transactions.end(), [&]( const full_transaction_ptr& transaction ) {
          return peer.was_requested( transaction->get_legacy_transaction_message_hash() );
        } );
      };

      auto staller = std::make_unique< test_peer >( peer_thread, db->get_chain_id(), "staller" );
      staller->connect( node_endpoint() );
      BOOST_REQUIRE( wait_until( [&]() { return staller->in_sync(); }, fc::seconds( 10 ) ) );

      // the node learns from these that the staller sends small transactions, so its window grows past one request
      const std::vector< full_transaction_ptr > warmup = transfers( "warmup", 150 );
      staller->provide( warmup );
      staller->advertise( hashes_of( warmup ) );
      BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return all_advertised( observer, warmup ); }, fc::seconds( 30 ) ),
                             "the warmup transactions were not accepted and relayed" );

      const std::vector< full_transaction_ptr > withheld = transfers( "withheld", int( GRAPHENE_NET_MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION + 200 ) );
      staller->advertise( hashes_of( withheld ) );
      BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return all_requested( *staller, withheld ); }, fc::seconds( 10 ) ),
                             "more transactions than one request may ask for were not all requested before any was delivered" );

      sender.provide( withheld );
      sender.advertise( hashes_of( withheld ) );
      fc::usleep( fc::milliseconds( 500 ) );
      BOOST_CHECK_MESSAGE( none_requested( sender, withheld ), "transactions outstanding with the staller were requested again" );

      staller.reset();
      BOOST_CHECK_MESSAGE( wait_until( [&]() { return all_advertised( observer, withheld ); }, fc::seconds( 30 ) ),
                           "transactions outstanding with a peer that left were not all fetched from another peer that advertised them" );
      BOOST_CHECK( all_requested( sender, withheld ) );
    } );
  }
  FC_LOG_AND_RETHROW()
}

/// A peer asking in one request for more transaction bytes than the node's send queue holds for it stays
/// connected and gets every one of them, each once, as the queue drains.
BOOST_AUTO_TEST_CASE( request_larger_than_the_send_queue_is_answered_in_full )
{
  try
  {
    run( {}, [&]( fc::thread&, test_peer& sender, test_peer& observer )
    {
      std::vector< full_transaction_ptr > large;
      for( int i = 0; i < 700; ++i )
      {
        const std::string memo = "large " + std::to_string( i ) + " ";
        large.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                         memo + std::string( HIVE_MAX_MEMO_SIZE - 1 - memo.size(), 'x' ), get_default_expiration() ) );
      }
      size_t requested_bytes = 0;
      for( const full_transaction_ptr& transaction : large )
        requested_bytes += transaction->get_transaction_size();
      BOOST_REQUIRE_GT( requested_bytes, size_t( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES ) );

      sender.provide( large );
      sender.advertise( hashes_of( large ) );
      BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return all_advertised( observer, large ); }, fc::seconds( 60 ) ),
                             "the large transactions were not accepted and relayed" );

      observer.send( graphene::net::fetch_items_message( graphene::net::trx_message_type, hashes_of( large ) ) );
      BOOST_CHECK_MESSAGE( observer.wait_for( graphene::net::trx_message_type, fc::seconds( 30 ), large.size() ),
                           "only " << observer.count( graphene::net::trx_message_type ) << " of " << large.size() << " transactions arrived" );
      fc::usleep( fc::milliseconds( 500 ) );

      BOOST_CHECK( !observer.closed() );
      BOOST_CHECK_EQUAL( observer.count( graphene::net::item_not_available_message_type ), 0u );
      std::set< graphene::net::item_hash_t > received;
      for( const p2p_tests::received_message& r : observer.received( graphene::net::trx_message_type ) )
        BOOST_CHECK( received.insert( r.msg.id() ).second );
      const std::vector< graphene::net::item_hash_t > requested = hashes_of( large );
      BOOST_CHECK( received == std::set< graphene::net::item_hash_t >( requested.begin(), requested.end() ) );
    } );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif

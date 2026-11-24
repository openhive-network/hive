#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/witness_fixture.hpp"
#include "../db_fixture/p2p_test_peer.hpp"

#include <hive/plugins/p2p/p2p_plugin.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/node_configuration.hpp>

#include <algorithm>
#include <deque>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
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

  /// Starts a node with @p extra_config_args and @p p2p_parameters, connects a sending and an observing peer to it and
  /// runs @p test with them on an API thread.
  void run( const config_arg_override_t& extra_config_args,
            const std::function< void( fc::thread& peer_thread, test_peer& sender, test_peer& observer ) >& test,
            const std::string& p2p_parameters = loopback_p2p_parameters )
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    config_arg_override_t config_args = { config_line_t( { "p2p-parameters", { p2p_parameters } } ) };
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
      // a new peer is first asked for only as many transactions as fit the request byte limit at the starting guess
      // of its transaction size (GRAPHENE_NET_INITIAL_TRANSACTION_SIZE_ESTIMATE), so it delivers just those before it
      // leaves
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
      staller->hello().defers_transaction_replies = true; // only such a peer is asked for more than one request
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

/// A peer that defers its replies is asked for more than one request's worth before it delivers any; one that doesn't
/// is asked for one request's worth at most, however small its transactions, as before requests were pipelined.
BOOST_AUTO_TEST_CASE( only_a_peer_that_defers_replies_is_asked_for_more_than_one_request )
{
  try
  {
    run( {}, [&]( fc::thread& peer_thread, test_peer&, test_peer& observer )
    {
      const auto transfers = [&]( const std::string& memo, int count )
      {
        std::vector< full_transaction_ptr > transactions;
        for( int i = 0; i < count; ++i )
          transactions.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                  memo + " " + std::to_string( i ), get_default_expiration() ) );
        return transactions;
      };
      const auto requested = [&]( const test_peer& peer, const std::vector< full_transaction_ptr >& transactions )
      {
        return size_t( std::count_if( transactions.begin(), transactions.end(), [&]( const full_transaction_ptr& transaction ) {
          return peer.was_requested( transaction->get_legacy_transaction_message_hash() );
        } ) );
      };
      const size_t one_request = GRAPHENE_NET_MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION;

      for( const bool defers : { false, true } )
      {
        const std::string name = defers ? "deferring" : "old";
        test_peer peer( peer_thread, db->get_chain_id(), name );
        peer.hello().defers_transaction_replies = defers;
        peer.connect( node_endpoint() );
        BOOST_REQUIRE( wait_until( [&]() { return peer.in_sync(); }, fc::seconds( 10 ) ) );

        // the node learns from these that the peer sends small transactions, so a request may ask for a full one
        const std::vector< full_transaction_ptr > warmup = transfers( name + " warmup", 150 );
        peer.provide( warmup );
        peer.advertise( hashes_of( warmup ) );
        BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return all_advertised( observer, warmup ); }, fc::seconds( 30 ) ),
                               "the warmup transactions were not accepted and relayed" );

        const std::vector< full_transaction_ptr > withheld = transfers( name + " withheld", int( one_request + 200 ) );
        peer.advertise( hashes_of( withheld ) );
        if( defers )
          BOOST_CHECK_MESSAGE( wait_until( [&]() { return requested( peer, withheld ) == withheld.size(); }, fc::seconds( 10 ) ),
                               "only " << requested( peer, withheld ) << " of " << withheld.size() << " transactions were requested from a peer that defers its replies" );
        else
        {
          BOOST_REQUIRE( wait_until( [&]() { return requested( peer, withheld ) > 0; }, fc::seconds( 10 ) ) );
          fc::usleep( fc::seconds( 1 ) );
          BOOST_CHECK_EQUAL( requested( peer, withheld ), one_request );
        }
        // the withheld transactions time out with this peer, which is dropped, before the next one connects
        BOOST_CHECK( wait_until( [&]() { return peer.closed(); }, fc::seconds( 15 ) ) );
      }
    } );
  }
  FC_LOG_AND_RETHROW()
}

/// A peer delivering the transactions it was asked for one at a time, slower than the request timeout per
/// request, is kept: its requests time out only once it stops delivering.  A peer delivering nothing is dropped
/// at the timeout.
BOOST_AUTO_TEST_CASE( peer_delivering_steadily_is_kept_past_the_request_timeout )
{
  try
  {
    const fc::microseconds timeout = fc::seconds( 2 );
    const fc::microseconds ceiling = fc::microseconds( timeout.count() * GRAPHENE_NET_MAX_TRANSACTION_REQUEST_TIMEOUTS_WHILE_DELIVERING );
    const std::string p2p_parameters =
      R"({"listen_endpoint":"127.0.0.1:0","accept_incoming_connections":true,"active_ignored_request_timeout_microseconds":)" +
      std::to_string( timeout.count() ) + "}";
    run( {}, [&]( fc::thread& peer_thread, test_peer&, test_peer& observer )
    {
      // as many as the first request to a new peer asks for (sized by GRAPHENE_NET_INITIAL_TRANSACTION_SIZE_ESTIMATE), so
      // however the node batches its requests no more than these wait at once, and delivering them in request order
      // one per pace keeps every wait within transaction_count * pace, while the last of a full request waits
      // more than the timeout
      const int64_t transaction_count = GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH / GRAPHENE_NET_INITIAL_TRANSACTION_SIZE_ESTIMATE;
      const fc::microseconds pace = fc::milliseconds( 350 );
      BOOST_REQUIRE_GT( ( transaction_count - 1 ) * pace.count(), timeout.count() );
      BOOST_REQUIRE_LE( transaction_count * pace.count(), ceiling.count() / 2 );
      std::vector< full_transaction_ptr > transactions;
      for( int64_t i = 0; i < transaction_count; ++i )
        transactions.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                "trickled " + std::to_string( i ), get_default_expiration() ) );
      std::map< graphene::net::item_hash_t, full_transaction_ptr > by_hash;
      for( const full_transaction_ptr& transaction : transactions )
        by_hash[ transaction->get_legacy_transaction_message_hash() ] = transaction;

      test_peer trickler( peer_thread, db->get_chain_id(), "trickler" );
      std::mutex requests_mutex;
      std::deque< std::pair< graphene::net::item_hash_t, fc::time_point > > requested;
      trickler.on( graphene::net::fetch_items_message_type, [&]( test_peer&, const graphene::net::message& m )
      {
        const graphene::net::fetch_items_message request = m.as< graphene::net::fetch_items_message >();
        const fc::time_point now = fc::time_point::now();
        std::lock_guard< std::mutex > guard( requests_mutex );
        for( const graphene::net::item_hash_t& item : request.items_to_fetch )
          requested.emplace_back( item, now );
      } );
      trickler.connect( node_endpoint() );
      BOOST_REQUIRE( wait_until( [&]() { return trickler.in_sync(); }, fc::seconds( 10 ) ) );
      trickler.advertise( hashes_of( transactions ) );

      const fc::time_point start = fc::time_point::now();
      fc::microseconds longest_wait;
      for( size_t delivered = 0; delivered < transactions.size() && !trickler.closed(); )
      {
        fc::optional< std::pair< graphene::net::item_hash_t, fc::time_point > > next;
        {
          std::lock_guard< std::mutex > guard( requests_mutex );
          if( !requested.empty() )
          {
            next = requested.front();
            requested.pop_front();
          }
        }
        if( next )
        {
          trickler.send( graphene::net::trx_message( by_hash.at( next->first ) ) );
          longest_wait = std::max( longest_wait, fc::time_point::now() - next->second );
          ++delivered;
        }
        fc::usleep( pace );
      }
      BOOST_TEST_MESSAGE( "delivered in " << ( fc::time_point::now() - start ).count() / 1000 << " ms, the longest after waiting "
                          << longest_wait.count() / 1000 << " ms, timeout " << timeout.count() / 1000 << " ms, ceiling "
                          << ceiling.count() / 1000 << " ms" );
      BOOST_CHECK_MESSAGE( !trickler.closed(), "a peer delivering its replies steadily was dropped" );
      BOOST_CHECK_MESSAGE( longest_wait > timeout, "no transaction waited longer than the timeout for its reply" );
      BOOST_CHECK_MESSAGE( longest_wait < ceiling, "a transaction waited past the ceiling for its reply" );
      BOOST_CHECK_MESSAGE( wait_until( [&]() { return all_advertised( observer, transactions ); }, fc::seconds( 10 ) ),
                           "the trickled transactions were not accepted and relayed" );
      BOOST_CHECK( !trickler.closed() );

      // a peer that delivers nothing it is asked for is dropped once the timeout passes
      const full_transaction_ptr ignored = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                           "ignored", get_default_expiration() );
      test_peer ignorer( peer_thread, db->get_chain_id(), "ignorer" );
      ignorer.connect( node_endpoint() );
      BOOST_REQUIRE( wait_until( [&]() { return ignorer.in_sync(); }, fc::seconds( 10 ) ) );
      ignorer.advertise( { ignored->get_legacy_transaction_message_hash() } );
      BOOST_REQUIRE( wait_until( [&]() { return ignorer.was_requested( ignored->get_legacy_transaction_message_hash() ); }, fc::seconds( 10 ) ) );
      BOOST_CHECK_MESSAGE( wait_until( [&]() { return ignorer.closed(); }, timeout + fc::seconds( 5 ) ),
                           "a peer that delivered nothing it was asked for was not dropped" );
    }, p2p_parameters );
  }
  FC_LOG_AND_RETHROW()
}

/// A peer that says it defers its replies, is asked for transactions another peer also advertised and keeps the
/// connection alive by answering one other request every few seconds is dropped once the ceiling on the wait passes,
/// and the transactions it held are fetched from the other advertiser while the node still knows it has them.
BOOST_AUTO_TEST_CASE( transactions_held_by_a_trickling_peer_are_fetched_from_another_advertiser )
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
      const std::vector< full_transaction_ptr > withheld = transfers( "held", 4 );
      const std::vector< full_transaction_ptr > decoys = transfers( "trickled", 12 );
      std::map< graphene::net::item_hash_t, full_transaction_ptr > decoys_by_hash;
      for( const full_transaction_ptr& transaction : decoys )
        decoys_by_hash[ transaction->get_legacy_transaction_message_hash() ] = transaction;

      test_peer trickler( peer_thread, db->get_chain_id(), "trickler" );
      trickler.hello().defers_transaction_replies = true;
      std::mutex requests_mutex;
      std::set< graphene::net::item_hash_t > requested;
      std::deque< graphene::net::item_hash_t > decoys_requested;
      trickler.on( graphene::net::fetch_items_message_type, [&]( test_peer&, const graphene::net::message& m )
      {
        const graphene::net::fetch_items_message request = m.as< graphene::net::fetch_items_message >();
        std::lock_guard< std::mutex > guard( requests_mutex );
        for( const graphene::net::item_hash_t& hash : request.items_to_fetch )
        {
          requested.insert( hash );
          if( decoys_by_hash.count( hash ) )
            decoys_requested.push_back( hash );
        }
      } );
      const auto all_requested_from_trickler = [&]( const std::vector< full_transaction_ptr >& transactions )
      {
        std::lock_guard< std::mutex > guard( requests_mutex );
        return std::all_of( transactions.begin(), transactions.end(), [&]( const full_transaction_ptr& transaction ) {
          return requested.count( transaction->get_legacy_transaction_message_hash() ) != 0;
        } );
      };
      trickler.connect( node_endpoint() );
      BOOST_REQUIRE( wait_until( [&]() { return trickler.in_sync(); }, fc::seconds( 10 ) ) );

      trickler.advertise( hashes_of( withheld ) );
      BOOST_REQUIRE( wait_until( [&]() { return all_requested_from_trickler( withheld ); }, fc::seconds( 10 ) ) );
      const fc::time_point requested_at = fc::time_point::now();
      sender.provide( withheld );
      sender.advertise( hashes_of( withheld ) );
      trickler.advertise( hashes_of( decoys ) );
      fc::usleep( fc::milliseconds( 500 ) );
      BOOST_CHECK_MESSAGE( std::none_of( withheld.begin(), withheld.end(), [&]( const full_transaction_ptr& transaction ) {
                             return sender.was_requested( transaction->get_legacy_transaction_message_hash() ); } ),
                           "transactions outstanding with the trickler were requested again" );

      // one reply every half timeout keeps the trickler from timing out for lack of progress
      const fc::microseconds pace = fc::seconds( 3 );
      size_t delivered = 0;
      while( !trickler.closed() && fc::time_point::now() < requested_at + fc::seconds( 45 ) )
      {
        fc::usleep( pace );
        fc::optional< graphene::net::item_hash_t > next;
        {
          std::lock_guard< std::mutex > guard( requests_mutex );
          if( !decoys_requested.empty() )
          {
            next = decoys_requested.front();
            decoys_requested.pop_front();
          }
        }
        if( next && !trickler.closed() )
        {
          trickler.send( graphene::net::trx_message( decoys_by_hash.at( *next ) ) );
          ++delivered;
        }
      }
      BOOST_REQUIRE_MESSAGE( trickler.closed(), "a peer holding transactions while trickling others was not dropped" );
      const fc::microseconds held_for = trickler.closed_at() - requested_at;
      BOOST_TEST_MESSAGE( "trickler delivered " << delivered << " and was dropped after " << held_for.count() / 1000 << " ms" );
      const fc::microseconds timeout = fc::microseconds( graphene::net::node_configuration().active_ignored_request_timeout_microseconds );
      BOOST_CHECK_GT( held_for.count(), 2 * timeout.count() ); // kept past the timeout by its replies
      BOOST_CHECK_LT( held_for.count(), ( GRAPHENE_NET_MAX_TRANSACTION_REQUEST_TIMEOUTS_WHILE_DELIVERING + 2 ) * timeout.count() );

      BOOST_CHECK_MESSAGE( wait_until( [&]() { return all_advertised( observer, withheld ); }, fc::seconds( 10 ) ),
                           "transactions held by a dropped trickler were not fetched from the other advertiser" );
      BOOST_CHECK_LT( ( fc::time_point::now() - requested_at ).count(),
                      fc::minutes( GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES ).count() );
      BOOST_CHECK( std::all_of( withheld.begin(), withheld.end(), [&]( const full_transaction_ptr& transaction ) {
        return sender.was_requested( transaction->get_legacy_transaction_message_hash() ); } ) );
    } );
  }
  FC_LOG_AND_RETHROW()
}

/// A peer that answers the other transactions it is asked for as not available, steadily, delivers nothing: the one it
/// holds times out at the request timeout, not at the ceiling a peer delivering others gets.
BOOST_AUTO_TEST_CASE( not_available_answers_do_not_keep_a_peer_holding_a_transaction )
{
  try
  {
    run( {}, [&]( fc::thread& peer_thread, test_peer&, test_peer& )
    {
      const graphene::net::item_hash_t held = fc::ripemd160::hash( std::string( "held" ) );
      std::vector< graphene::net::item_hash_t > refused;
      for( int i = 0; i < 24; ++i )
        refused.push_back( fc::ripemd160::hash( "refused " + std::to_string( i ) ) );

      test_peer refuser( peer_thread, db->get_chain_id(), "refuser" );
      refuser.hello().defers_transaction_replies = true;
      std::mutex requests_mutex;
      std::deque< graphene::net::item_hash_t > refused_requested;
      bool held_requested = false;
      refuser.on( graphene::net::fetch_items_message_type, [&]( test_peer&, const graphene::net::message& m )
      {
        const graphene::net::fetch_items_message request = m.as< graphene::net::fetch_items_message >();
        std::lock_guard< std::mutex > guard( requests_mutex );
        for( const graphene::net::item_hash_t& hash : request.items_to_fetch )
          if( hash == held )
            held_requested = true;
          else
            refused_requested.push_back( hash );
      } );
      refuser.connect( node_endpoint() );
      BOOST_REQUIRE( wait_until( [&]() { return refuser.in_sync(); }, fc::seconds( 10 ) ) );

      std::vector< graphene::net::item_hash_t > advertised{ held };
      advertised.insert( advertised.end(), refused.begin(), refused.end() );
      refuser.advertise( advertised );
      BOOST_REQUIRE( wait_until( [&]() { std::lock_guard< std::mutex > guard( requests_mutex ); return held_requested; }, fc::seconds( 10 ) ) );
      const fc::time_point requested_at = fc::time_point::now();

      // one "not available" every half second would have kept it until the ceiling
      size_t answered = 0;
      while( !refuser.closed() && fc::time_point::now() < requested_at + fc::seconds( 30 ) )
      {
        fc::usleep( fc::milliseconds( 500 ) );
        fc::optional< graphene::net::item_hash_t > next;
        {
          std::lock_guard< std::mutex > guard( requests_mutex );
          if( !refused_requested.empty() )
          {
            next = refused_requested.front();
            refused_requested.pop_front();
          }
        }
        if( next && !refuser.closed() )
        {
          refuser.send( graphene::net::item_not_available_message( graphene::net::item_id( graphene::net::trx_message_type, *next ) ) );
          ++answered;
        }
      }
      BOOST_REQUIRE_MESSAGE( refuser.closed(), "a peer holding a transaction and refusing the others was not dropped" );
      const fc::microseconds held_for = refuser.closed_at() - requested_at;
      BOOST_TEST_MESSAGE( "refuser answered " << answered << " as not available and was dropped after " << held_for.count() / 1000 << " ms" );
      const fc::microseconds timeout = fc::microseconds( graphene::net::node_configuration().active_ignored_request_timeout_microseconds );
      BOOST_CHECK_GE( answered, size_t( timeout.count() / 1000000 ) ); // it was answering steadily until it was dropped
      BOOST_CHECK_LT( held_for.count(), timeout.count() + fc::seconds( 3 ).count() );
    } );
  }
  FC_LOG_AND_RETHROW()
}

/// Transactions another peer advertised long before a trickling peer is asked for them, which waited in the queue to
/// fetch meanwhile while both peers were kept busy, are fetched from that other peer once the trickler is dropped:
/// asking the trickler for them renews the record of who advertised them, which would otherwise expire (the
/// inventory lifetime after they were advertised) before the trickler's ceiling passes.  The peers are kept busy by
/// the node's own demotion of a peer that answers items it advertised as not available
/// (GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC), batch after batch.
BOOST_AUTO_TEST_CASE( transactions_queued_before_a_trickler_is_asked_are_fetched_from_the_earlier_advertiser )
{
  try
  {
    run( {}, [&]( fc::thread& peer_thread, test_peer& advertiser, test_peer& observer )
    {
      const auto transfers = [&]( const std::string& memo, int count )
      {
        std::vector< full_transaction_ptr > transactions;
        for( int i = 0; i < count; ++i )
          transactions.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                  memo + " " + std::to_string( i ), get_default_expiration() ) );
        return transactions;
      };
      // each peer is asked for its items in requests of a full window: 8, as each request ends in a transaction of
      // about GRAPHENE_NET_INITIAL_TRANSACTION_SIZE_ESTIMATE that it delivers at once, so its estimate stays near the
      // initial one.  The rest of each request are fillers it answers as not available @p pace later, which demotes it
      // for GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC; the delivery before keeps that from counting toward a disconnect.
      // So each request comes pace + demotion after the one before, and the batches before the withheld transactions
      // keep the trickler from them for about batches * (4 s + 10 s): past what the ceiling needs (lifetime - 18 s),
      // short of the lifetime
      const size_t batches = 4;
      const size_t window = GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH / GRAPHENE_NET_INITIAL_TRANSACTION_SIZE_ESTIMATE;
      const auto fillers = [&]( const std::string& name )
      {
        std::vector< graphene::net::item_hash_t > hashes;
        for( size_t i = 0; i < batches * ( window - 1 ); ++i )
          hashes.push_back( fc::ripemd160::hash( name + " filler " + std::to_string( i ) ) );
        return hashes;
      };
      const auto large_transactions = [&]( const std::string& name )
      {
        std::vector< full_transaction_ptr > transactions;
        for( size_t b = 0; b < batches; ++b )
        {
          signed_transaction tx;
          db->with_read_lock( [&]()
          {
            tx.set_expiration( db->head_block_time() + get_default_expiration() );
            tx.set_reference_block( db->head_block_id() );
          } );
          const std::string prefix = name + " large " + std::to_string( b ) + " ";
          for( int i = 0; i < 30; ++i )
          {
            transfer_operation transfer;
            transfer.from = HIVE_INIT_MINER_NAME;
            transfer.to = HIVE_NULL_ACCOUNT;
            transfer.amount = ASSET( "0.001 TESTS" );
            transfer.memo = prefix + std::to_string( i ) + std::string( 2000, 'x' );
            tx.operations.emplace_back( transfer );
          }
          transactions.push_back( build_transaction( tx ) );
          BOOST_REQUIRE_LE( transactions.back()->get_transaction_size(), size_t( GRAPHENE_NET_INITIAL_TRANSACTION_SIZE_ESTIMATE ) );
          BOOST_REQUIRE_GT( transactions.back()->get_transaction_size(), size_t( GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH / ( window + 1 ) ) );
        }
        return transactions;
      };
      const std::vector< full_transaction_ptr > withheld = transfers( "queued", 4 );
      const std::vector< full_transaction_ptr > decoys = transfers( "trickled", 12 );
      std::map< graphene::net::item_hash_t, full_transaction_ptr > decoys_by_hash;
      for( const full_transaction_ptr& transaction : decoys )
        decoys_by_hash[ transaction->get_legacy_transaction_message_hash() ] = transaction;
      const std::vector< graphene::net::item_hash_t > advertiser_filler_list = fillers( "advertiser" );
      const std::vector< graphene::net::item_hash_t > trickler_filler_list = fillers( "trickler" );
      const std::set< graphene::net::item_hash_t > advertiser_fillers( advertiser_filler_list.begin(), advertiser_filler_list.end() );
      const std::set< graphene::net::item_hash_t > trickler_fillers( trickler_filler_list.begin(), trickler_filler_list.end() );
      const std::vector< full_transaction_ptr > advertiser_large = large_transactions( "advertiser" );
      const std::vector< full_transaction_ptr > trickler_large = large_transactions( "trickler" );
      std::map< graphene::net::item_hash_t, full_transaction_ptr > trickler_large_by_hash;
      for( const full_transaction_ptr& transaction : trickler_large )
        trickler_large_by_hash[ transaction->get_legacy_transaction_message_hash() ] = transaction;
      const auto refuse_fillers_after = []( const std::set< graphene::net::item_hash_t >& fillers, fc::microseconds pace )
      {
        return [&fillers, pace]( test_peer& self, const graphene::net::fetch_items_message& request )
        {
          std::vector< graphene::net::item_hash_t > refused;
          for( const graphene::net::item_hash_t& hash : request.items_to_fetch )
            if( fillers.count( hash ) )
              refused.push_back( hash );
          if( !refused.empty() )
            self.later( pace, [&self, refused]()
            {
              for( const graphene::net::item_hash_t& hash : refused )
                self.send( graphene::net::item_not_available_message( graphene::net::item_id( graphene::net::trx_message_type, hash ) ) );
            } );
        };
      };
      // in batches of the window: window - 1 fillers, then one large transaction; then @p transactions
      const auto filler_hashes_then = [&]( const std::vector< graphene::net::item_hash_t >& fillers, const std::vector< full_transaction_ptr >& large,
                                           const std::vector< full_transaction_ptr >& transactions )
      {
        std::vector< graphene::net::item_hash_t > hashes;
        for( size_t b = 0; b < batches; ++b )
        {
          hashes.insert( hashes.end(), fillers.begin() + b * ( window - 1 ), fillers.begin() + ( b + 1 ) * ( window - 1 ) );
          hashes.push_back( large[ b ]->get_legacy_transaction_message_hash() );
        }
        const std::vector< graphene::net::item_hash_t > rest = hashes_of( transactions );
        hashes.insert( hashes.end(), rest.begin(), rest.end() );
        return hashes;
      };

      // the advertiser delivers what it is asked for but its fillers, refused at the slower pace, so the trickler
      // reaches the withheld transactions first
      const auto refuse_advertiser_fillers = refuse_fillers_after( advertiser_fillers, fc::seconds( 5 ) );
      advertiser.on( graphene::net::fetch_items_message_type, [&]( test_peer& self, const graphene::net::message& m )
      {
        graphene::net::fetch_items_message request = m.as< graphene::net::fetch_items_message >();
        refuse_advertiser_fillers( self, request );
        request.items_to_fetch.erase( std::remove_if( request.items_to_fetch.begin(), request.items_to_fetch.end(),
                                                      [&]( const graphene::net::item_hash_t& hash ) { return advertiser_fillers.count( hash ) != 0; } ),
                                      request.items_to_fetch.end() );
        if( !request.items_to_fetch.empty() )
          self.answer( graphene::net::message( request ) );
      } );

      test_peer trickler( peer_thread, db->get_chain_id(), "trickler" );
      trickler.hello().defers_transaction_replies = true;
      std::mutex requests_mutex;
      std::set< graphene::net::item_hash_t > requested;
      std::deque< graphene::net::item_hash_t > decoys_requested;
      const auto refuse_trickler_fillers = refuse_fillers_after( trickler_fillers, fc::seconds( 4 ) );
      trickler.on( graphene::net::fetch_items_message_type, [&]( test_peer& self, const graphene::net::message& m )
      {
        const graphene::net::fetch_items_message request = m.as< graphene::net::fetch_items_message >();
        refuse_trickler_fillers( self, request );
        for( const graphene::net::item_hash_t& hash : request.items_to_fetch )
          if( trickler_large_by_hash.count( hash ) )
            self.send( graphene::net::trx_message( trickler_large_by_hash.at( hash ) ) );
        std::lock_guard< std::mutex > guard( requests_mutex );
        for( const graphene::net::item_hash_t& hash : request.items_to_fetch )
        {
          requested.insert( hash );
          if( decoys_by_hash.count( hash ) )
            decoys_requested.push_back( hash );
        }
      } );
      const auto all_requested_from_trickler = [&]( const std::vector< full_transaction_ptr >& transactions )
      {
        std::lock_guard< std::mutex > guard( requests_mutex );
        return std::all_of( transactions.begin(), transactions.end(), [&]( const full_transaction_ptr& transaction ) {
          return requested.count( transaction->get_legacy_transaction_message_hash() ) != 0;
        } );
      };
      trickler.connect( node_endpoint() );
      BOOST_REQUIRE( wait_until( [&]() { return trickler.in_sync(); }, fc::seconds( 10 ) ) );

      const fc::time_point advertised_at = fc::time_point::now();
      advertiser.provide( withheld );
      advertiser.provide( advertiser_large );
      advertiser.advertise( filler_hashes_then( advertiser_filler_list, advertiser_large, withheld ) );
      trickler.advertise( filler_hashes_then( trickler_filler_list, trickler_large, withheld ) );
      trickler.advertise( hashes_of( decoys ) );

      BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return all_requested_from_trickler( withheld ); }, fc::seconds( 70 ) ),
                             "the withheld transactions were not requested from the trickler" );
      const fc::time_point requested_at = fc::time_point::now();
      BOOST_TEST_MESSAGE( "the trickler was asked for the withheld transactions " << ( requested_at - advertised_at ).count() / 1000 << " ms after they were advertised" );
      BOOST_CHECK_MESSAGE( std::none_of( withheld.begin(), withheld.end(), [&]( const full_transaction_ptr& transaction ) {
                             return advertiser.was_requested( transaction->get_legacy_transaction_message_hash() ); } ),
                           "the advertiser was asked for the withheld transactions before the trickler" );

      // one reply every half timeout keeps the trickler from timing out for lack of progress
      const fc::microseconds pace = fc::seconds( 3 );
      while( !trickler.closed() && fc::time_point::now() < requested_at + fc::seconds( 45 ) )
      {
        fc::usleep( pace );
        fc::optional< graphene::net::item_hash_t > next;
        {
          std::lock_guard< std::mutex > guard( requests_mutex );
          if( !decoys_requested.empty() )
          {
            next = decoys_requested.front();
            decoys_requested.pop_front();
          }
        }
        if( next && !trickler.closed() )
          trickler.send( graphene::net::trx_message( decoys_by_hash.at( *next ) ) );
      }
      BOOST_REQUIRE_MESSAGE( trickler.closed(), "a peer holding transactions while trickling others was not dropped" );
      const fc::microseconds lifetime = fc::minutes( GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES );
      BOOST_TEST_MESSAGE( "the trickler was dropped " << ( trickler.closed_at() - advertised_at ).count() / 1000 << " ms after they were advertised" );
      // the case keep() is for: the records of who advertised them would have expired by then
      BOOST_REQUIRE_GT( ( trickler.closed_at() - advertised_at ).count(), lifetime.count() + fc::seconds( 2 ).count() );

      BOOST_CHECK_MESSAGE( wait_until( [&]() { return all_advertised( observer, withheld ); }, fc::seconds( 15 ) ),
                           "transactions held by a dropped trickler were not fetched from the peer that advertised them before it was asked" );
      BOOST_CHECK( std::all_of( withheld.begin(), withheld.end(), [&]( const full_transaction_ptr& transaction ) {
        return advertiser.was_requested( transaction->get_legacy_transaction_message_hash() ); } ) );
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

/// On a chain whose witnesses voted blocks over the send queue's limit, a transaction larger than the limit (it may
/// be up to maximum_block_size - 256 bytes) is relayed from the peer that sent it, through the node, to a peer that
/// asks the node for it, and is included in a block: neither the node nor its peers close a connection on their
/// send queue's limit.
BOOST_AUTO_TEST_CASE( transaction_over_the_send_queue_limit_is_relayed )
{
  try
  {
    run( {}, [&]( fc::thread&, test_peer& sender, test_peer& observer )
    {
      witness_set_properties_operation raise_block_size;
      raise_block_size.owner = HIVE_INIT_MINER_NAME;
      raise_block_size.props[ "key" ] = fc::raw::pack_to_vector( init_account_pub_key );
      raise_block_size.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( uint32_t( HIVE_MAX_BLOCK_SIZE ) );
      schedule_transaction( raise_block_size );
      // initminer is the only witness, so its vote is the median once the next schedules take it
      BOOST_REQUIRE_MESSAGE( wait_until( [&]()
      {
        uint32_t maximum_block_size = 0;
        db->with_read_lock( [&]() { maximum_block_size = db->get_dynamic_global_properties().get_maximum_block_size(); } );
        return maximum_block_size == HIVE_MAX_BLOCK_SIZE;
      }, fc::seconds( 3 * HIVE_BLOCK_INTERVAL * HIVE_MAX_WITNESSES ) ), "the voted maximum block size did not take effect" );

      comment_operation post;
      post.parent_permlink = "test";
      post.author = HIVE_INIT_MINER_NAME;
      post.permlink = "larger-than-the-send-queue";
      post.title = "large";
      post.body = std::string( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES * 3 / 2, 'x' );
      const full_transaction_ptr large = build_transaction( post, get_default_expiration() );
      BOOST_REQUIRE_GT( large->get_transaction_size(), size_t( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES ) );
      BOOST_REQUIRE_LE( large->get_transaction_size(), size_t( HIVE_MAX_BLOCK_SIZE - 256 ) );

      uint32_t checked_through = get_block_num();
      sender.provide( { large } );
      sender.advertise( hashes_of( { large } ) );
      BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return all_advertised( observer, { large } ); }, fc::seconds( 30 ) ),
                             "the large transaction was not accepted and relayed" );
      observer.send( graphene::net::fetch_items_message( graphene::net::trx_message_type, hashes_of( { large } ) ) );
      BOOST_CHECK_MESSAGE( observer.wait_for( graphene::net::trx_message_type, fc::seconds( 30 ) ),
                           "the node did not send the large transaction to the peer that asked for it" );
      BOOST_CHECK_MESSAGE( wait_until( [&]()
      {
        const uint32_t head = get_block_num();
        for( ; checked_through < head; ++checked_through )
        {
          const std::shared_ptr< full_block_type > block = get_block_reader().get_block_by_number( checked_through + 1 );
          BOOST_REQUIRE( block );
          for( const full_transaction_ptr& transaction : block->get_full_transactions() )
            if( transaction->get_transaction_id() == large->get_transaction_id() )
              return true;
        }
        return false;
      }, fc::seconds( 30 ) ), "the large transaction was not included in a block" );

      BOOST_CHECK( !sender.closed() );
      BOOST_CHECK( !observer.closed() );
      BOOST_CHECK_EQUAL( observer.count( graphene::net::item_not_available_message_type ), 0u );
      const std::vector< p2p_tests::received_message > received = observer.received( graphene::net::trx_message_type );
      BOOST_REQUIRE_EQUAL( received.size(), 1u );
      BOOST_CHECK( received.front().msg.id() == large->get_legacy_transaction_message_hash() );
    } );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif

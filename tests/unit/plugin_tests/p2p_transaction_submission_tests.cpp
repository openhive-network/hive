#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/witness_fixture.hpp"
#include "p2p_test_peer.hpp"

#include <hive/plugins/p2p/p2p_plugin.hpp>

#include <functional>
#include <memory>
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

        const fc::ip::endpoint node_endpoint =
          theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_info()[ "listening_on" ].as< fc::ip::endpoint >();
        fc::thread peer_thread( "test_peers" );
        test_peer sender( peer_thread, db->get_chain_id(), "sender" );
        test_peer observer( peer_thread, db->get_chain_id(), "observer" );
        for( test_peer* peer : { &sender, &observer } )
        {
          peer->connect( node_endpoint );
          BOOST_REQUIRE( wait_until( [&]() { return peer->in_sync(); }, fc::seconds( 10 ) ) );
        }
        // the node advertises transactions only to peers it has no blocks to offer
        BOOST_REQUIRE( wait_until( [&]()
        {
          if( observer.node_considers_us_in_sync() )
            return true;
          block_id_type head;
          db->with_read_lock( [&]() { head = db->head_block_id(); } );
          observer.tell_head( head );
          fc::usleep( fc::milliseconds( 200 ) );
          return observer.node_considers_us_in_sync();
        }, fc::seconds( 10 ) ) );

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
      leaving.connect( theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_info()[ "listening_on" ].as< fc::ip::endpoint >() );
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
      BOOST_CHECK_MESSAGE( wait_until( [&]() { return all_advertised( observer, left_behind ); }, fc::seconds( 10 ) ),
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

BOOST_AUTO_TEST_SUITE_END()

#endif

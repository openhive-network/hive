#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/witness_fixture.hpp"

#include <hive/chain/full_block.hpp>
#include <hive/chain/notifications.hpp>
#include <hive/plugins/p2p/p2p_plugin.hpp>
#include <hive/utilities/signal.hpp>
#include <hive/utilities/tempdir.hpp>

#include <graphene/net/config.hpp>

#include <fc/filesystem.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace {

/// Every node listens on loopback and tells its peers about no other, so the line B makes stays one.
/// They all run on one host, which the per-host connection limit must not count against them.
const std::string line_p2p_parameters = R"({"listen_endpoint":"127.0.0.1:0","accept_incoming_connections":true,)"
  R"("peer_advertising_disabled":true,"maximum_connections_per_host":0})";

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

/// counts the "send queue exceeded maximum size" errors with which a node closes a connection on its send queue's limit
class send_queue_overflow_catcher : public fc::appender
{
public:
  void log( const fc::log_message& m ) override
  {
    if( m.get_format().find( "send queue exceeded maximum size" ) != std::string::npos )
      ++_count;
  }

  uint32_t count() const { return _count; }

private:
  std::atomic< uint32_t > _count = 0;
};

/// One hived node of the line, in its own data directory, sharing the logging of the first one started.
struct relay_node : public witness_fixture
{
  relay_node( const fc::path& data_dir ) { set_data_dir( data_dir.string() ); }

  void start( const std::vector< std::string >& represented_witnesses, const fc::optional< fc::logging_config >& logging )
  {
    set_logging_config( logging );
    initialize( 6, {}, represented_witnesses, { config_line_t( { "p2p-parameters", { line_p2p_parameters } } ) } );
  }

  void stop()
  {
    theApp.generate_interrupt_request();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
  }

  hive::plugins::p2p::p2p_plugin& p2p() { return theApp.get_plugin< hive::plugins::p2p::p2p_plugin >(); }

  fc::ip::endpoint endpoint() { return p2p().get_info()[ "listening_on" ].as< fc::ip::endpoint >(); }

  size_t peer_count() { return p2p().get_connected_peers().size(); }

  /// what the API of this node does with a transaction: apply it and advertise it to the peers
  void submit( const full_transaction_ptr& transaction )
  {
    get_chain_plugin().accept_transaction( transaction, hive::plugins::chain::chain_plugin::lock_type::fc );
    p2p().broadcast_transaction( transaction );
  }

  template< typename Lambda >
  auto read( Lambda&& callback ) const
  {
    return db->with_read_lock( std::forward< Lambda >( callback ) );
  }
};

/// The transactions a node applied on their own (from a peer: nothing calls its API) and the ones it applied in blocks,
/// with the ones it had not applied on their own before.
class transaction_watch
{
public:
  explicit transaction_watch( relay_node& node ) : _db( *node.db )
  {
    _transaction_conn = _db.add_post_apply_transaction_handler( [this]( const transaction_notification& note )
    {
      if( !_db.is_validating_one_tx() )
        return;
      std::lock_guard< std::mutex > guard( _mutex );
      _received.insert( note.transaction_id );
    }, node.get_chain_plugin(), 0 );
    _block_conn = _db.add_post_apply_block_handler( [this]( const block_notification& note )
    {
      std::lock_guard< std::mutex > guard( _mutex );
      for( const full_transaction_ptr& transaction : note.full_block->get_full_transactions() )
      {
        _included.insert( transaction->get_transaction_id() );
        if( _received.count( transaction->get_transaction_id() ) == 0 )
          _included_unreceived.insert( transaction->get_transaction_id() );
      }
    }, node.get_chain_plugin(), 0 );
  }

  ~transaction_watch()
  {
    hive::utilities::disconnect_signal( _transaction_conn );
    hive::utilities::disconnect_signal( _block_conn );
  }

  bool received( const transaction_id_type& id ) const { return contains( _received, id ); }
  bool included( const transaction_id_type& id ) const { return contains( _included, id ); }
  bool included_unreceived( const transaction_id_type& id ) const { return contains( _included_unreceived, id ); }

private:
  bool contains( const std::set< transaction_id_type >& ids, const transaction_id_type& id ) const
  {
    std::lock_guard< std::mutex > guard( _mutex );
    return ids.count( id ) != 0;
  }

  database& _db;
  mutable std::mutex _mutex;
  std::set< transaction_id_type > _received;
  std::set< transaction_id_type > _included;
  std::set< transaction_id_type > _included_unreceived;
  hive::utilities::signal_connection_ptr _transaction_conn;
  hive::utilities::signal_connection_ptr _block_conn;
};

/// The authority of rc_multisig_recover_account (rc_tests): "victim" needs all 40 "mix" accounts, each of them 38 keys
/// of its own and two "key" accounts of 40 keys each, so 4720 signatures in all.
struct multisig_authority
{
  std::vector< fc::ecc::private_key > keys;
  std::vector< account_create_operation > creates; ///< in order: each account after the accounts its authority names

  multisig_authority( const relay_node& node, const asset& fee )
  {
    std::vector< account_name_type > mixed;
    for( int i = 0; i < HIVE_MAX_AUTHORITY_MEMBERSHIP; ++i )
    {
      const account_name_type first = signer( node, fee, "key", 2 * i, HIVE_MAX_AUTHORITY_MEMBERSHIP, {} );
      const account_name_type second = signer( node, fee, "key", 2 * i + 1, HIVE_MAX_AUTHORITY_MEMBERSHIP, {} );
      mixed.push_back( signer( node, fee, "mix", i, HIVE_MAX_AUTHORITY_MEMBERSHIP - 2, { first, second } ) );
    }

    authority victim( HIVE_MAX_AUTHORITY_MEMBERSHIP );
    for( const account_name_type& name : mixed )
      victim.add_authority( name, 1 );
    creates.push_back( create( node, fee, "victim", victim ) );
  }

private:
  account_name_type signer( const relay_node& node, const asset& fee, const std::string& prefix, int n, int key_count,
    const std::vector< account_name_type >& accounts )
  {
    const std::string name = prefix + std::string( 4 - std::to_string( n ).size(), '0' ) + std::to_string( n );
    authority auth( HIVE_MAX_AUTHORITY_MEMBERSHIP );
    for( int k = 0; k < key_count; ++k )
    {
      keys.push_back( node.generate_private_key( name + "-" + std::to_string( k ) ) );
      auth.add_authority( keys.back().get_public_key(), 1 );
    }
    for( const account_name_type& account : accounts )
      auth.add_authority( account, 1 );
    creates.push_back( create( node, fee, name, auth ) );
    return name;
  }

  static account_create_operation create( const relay_node& node, const asset& fee, const std::string& name,
    const authority& active )
  {
    account_create_operation create;
    create.fee = fee;
    create.creator = HIVE_INIT_MINER_NAME;
    create.new_account_name = name;
    create.owner = authority( 1, node.init_account_pub_key, 1 );
    create.active = active;
    create.posting = create.owner;
    create.memo_key = node.init_account_pub_key;
    return create;
  }
};

} // namespace

// Disabled by default, as p2p_protocol_tests is: plugin-tests and plugin-api leave it out, and AIDEV's `p2p-relay` step
// (.aidev/run-checks.sh) runs it when the diff reaches p2p, the network code or the transaction size limits.
BOOST_AUTO_TEST_SUITE( p2p_relay_tests, * boost::unit_test::disabled() )

/// Three hived nodes in a line, A - B - C, on a chain whose only witness votes blocks of HIVE_MAX_BLOCK_SIZE. Three
/// transactions needing 4720 signatures each (over 300 kB) and a comment over the send queue's limit are submitted to A.
/// C, the only one producing blocks, can include them only after B relayed them to it, so the test checks that B and C
/// each received every one of them from its peer, that they are in blocks A received back through B, and that no node
/// closed a connection on its send queue's limit.
BOOST_AUTO_TEST_CASE( large_transactions_relayed_across_three_nodes )
{
  try
  {
    const auto started = std::chrono::steady_clock::now();
    fc::temp_directory dir_a( hive::utilities::temp_directory_path() );
    fc::temp_directory dir_b( hive::utilities::temp_directory_path() );
    fc::temp_directory dir_c( hive::utilities::temp_directory_path() );
    relay_node a( dir_a.path() ), b( dir_b.path() ), c( dir_c.path() );
    BOOST_SCOPE_EXIT( &a, &b, &c )
    {
      for( relay_node* node : { &a, &b, &c } )
        node->stop();
    } BOOST_SCOPE_EXIT_END

    c.start( { HIVE_INIT_MINER_NAME }, {} );
    a.set_genesis_time( c.get_genesis_time() );
    a.start( {}, c.get_logging_config() );
    b.set_genesis_time( c.get_genesis_time() );
    b.start( {}, c.get_logging_config() );
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages
    fc::logger::get( "p2p" ).set_log_level( fc::log_level::warn );

    bool test_passed = false;
    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
    {
      auto catcher = fc::shared_ptr< send_queue_overflow_catcher >( new send_queue_overflow_catcher() );
      fc::logger::get( "p2p" ).add_appender( catcher );
      BOOST_SCOPE_EXIT( catcher ) { fc::logger::get( "p2p" ).remove_appender( catcher ); } BOOST_SCOPE_EXIT_END
      try
      {
        // B dials both: a node asks the first peer it dials whether it is firewalled, and that peer has another one
        // dial it to find out, so had C dialed B, A would have dialed C
        b.p2p().add_node( a.endpoint() );
        b.p2p().add_node( c.endpoint() );
        BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return a.peer_count() == 1 && b.peer_count() == 2 && c.peer_count() == 1; },
          fc::seconds( 30 ) ), "the nodes did not connect in a line: A has " << a.peer_count() << " peers, B "
          << b.peer_count() << ", C " << c.peer_count() );

        fc::sleep_until( c.get_genesis_time() + HIVE_BLOCK_INTERVAL );
        c.wait_for_block_num( 1 );

        witness_set_properties_operation raise_block_size;
        raise_block_size.owner = HIVE_INIT_MINER_NAME;
        raise_block_size.props[ "key" ] = fc::raw::pack_to_vector( c.init_account_pub_key );
        raise_block_size.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( uint32_t( HIVE_MAX_BLOCK_SIZE ) );
        c.schedule_transaction( raise_block_size );
        // the chain starts with blocks of HIVE_MAX_BLOCK_SIZE; initminer, the only witness, votes to keep them when the
        // schedule takes the votes at block HIVE_MAX_WITNESSES, after this test
        BOOST_REQUIRE_MESSAGE( wait_until( [&]()
        {
          return a.read( [&]()
          {
            return a.db->get_witness( HIVE_INIT_MINER_NAME ).props.maximum_block_size == HIVE_MAX_BLOCK_SIZE &&
              a.db->get_dynamic_global_properties().get_maximum_block_size() == HIVE_MAX_BLOCK_SIZE;
          } );
        }, fc::seconds( 30 ) ), "the vote for the maximum block size did not reach A" );

        const asset fee = c.read( [&]() { return c.db->get_witness_schedule_object().median_props.account_creation_fee.to_asset(); } );
        const multisig_authority multisig( c, fee );
        for( const account_create_operation& create : multisig.creates )
          c.schedule_transaction( create );
        c.schedule_vest( "victim", ASSET( "1000000.000 TESTS" ) );
        c.schedule_fund( "victim", ASSET( "1.000 TESTS" ) );
        BOOST_REQUIRE_MESSAGE( wait_until( [&]()
        {
          return a.read( [&]()
          {
            const account_object* victim = a.db->find_account( "victim" );
            return victim != nullptr && a.db->get_balance( *victim, HIVE_SYMBOL ).amount > 0;
          } );
        }, fc::seconds( 60 ) ), "the multisig accounts did not reach A" );
        BOOST_REQUIRE_EQUAL( multisig.keys.size(), 4720u );

        // signed once, outside the relay; each node verifies the signatures itself
        std::vector< full_transaction_ptr > large;
        for( int i = 0; i < 3; ++i )
        {
          transfer_operation transfer;
          transfer.from = "victim";
          transfer.to = HIVE_INIT_MINER_NAME;
          transfer.amount = ASSET( "0.001 TESTS" );
          transfer.memo = "multisig " + std::to_string( i );
          signed_transaction tx;
          a.read( [&]()
          {
            tx.set_expiration( a.db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION / 2 );
            tx.set_reference_block( a.db->head_block_id() );
          } );
          tx.operations.emplace_back( transfer );
          full_transaction_ptr signed_tx = full_transaction_type::create_from_signed_transaction( tx, serialization_type::hf26, false );
          signed_tx->sign_transaction( multisig.keys, a.db->get_chain_id(), serialization_type::hf26 );
          BOOST_REQUIRE_GT( signed_tx->get_transaction_size(), 300'000u );
          large.push_back( signed_tx );
        }
        comment_operation post;
        post.parent_permlink = "test";
        post.author = HIVE_INIT_MINER_NAME;
        post.permlink = "relayed-across-three-nodes";
        post.title = "large";
        post.body = std::string( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES + 100'000, 'x' );
        large.push_back( a.build_transaction( post, HIVE_MAX_TIME_UNTIL_EXPIRATION / 2 ) );
        BOOST_REQUIRE_GT( large.back()->get_transaction_size(), size_t( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES ) );
        BOOST_REQUIRE_LE( large.back()->get_transaction_size(), size_t( HIVE_MAX_BLOCK_SIZE - 256 ) );

        transaction_watch on_a( a ), on_b( b ), on_c( c );
        for( const full_transaction_ptr& transaction : large )
          a.submit( transaction );
        const auto all = [&]( const transaction_watch& watch, bool ( transaction_watch::*has )( const transaction_id_type& ) const )
        {
          return std::all_of( large.begin(), large.end(),
            [&]( const full_transaction_ptr& transaction ) { return ( watch.*has )( transaction->get_transaction_id() ); } );
        };
        BOOST_CHECK_MESSAGE( wait_until( [&]() { return all( on_a, &transaction_watch::included ) || catcher->count() > 0; },
          fc::seconds( 60 ) ), "the large transactions did not come back to A in blocks" );

        for( size_t i = 0; i < large.size(); ++i )
        {
          const transaction_id_type& id = large[ i ]->get_transaction_id();
          ilog( "transaction ${i}: ${size} bytes", ( "i", i )( "size", large[ i ]->get_transaction_size() ) );
          BOOST_CHECK_MESSAGE( on_b.received( id ), "B did not receive transaction " << i << " from A" );
          BOOST_CHECK_MESSAGE( on_c.received( id ), "C did not receive transaction " << i << " from B" );
          BOOST_CHECK_MESSAGE( !on_c.included_unreceived( id ), "C applied transaction " << i << " in a block first" );
          BOOST_CHECK_MESSAGE( on_a.included( id ), "transaction " << i << " is in no block A received" );
        }
        BOOST_CHECK_MESSAGE( catcher->count() == 0, catcher->count() << " connections were closed on the send queue's limit" );
        BOOST_CHECK_EQUAL( a.peer_count(), 1u );
        BOOST_CHECK_EQUAL( b.peer_count(), 2u );
        BOOST_CHECK_EQUAL( c.peer_count(), 1u );
        test_passed = true;
      }
      CATCH( "API" )
    } );
    _future.wait();
    BOOST_REQUIRE( test_passed );
    ilog( "three nodes started, set up and relayed the large transactions in ${s} s", ( "s",
      std::chrono::duration_cast< std::chrono::seconds >( std::chrono::steady_clock::now() - started ).count() ) );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif

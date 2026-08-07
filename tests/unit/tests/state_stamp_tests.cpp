#include <boost/test/unit_test.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/notifications.hpp>
#include <hive/chain/detail/state/state_stamp_data_object_multiindex.hpp>

#include <hive/protocol/block.hpp>
#include <hive/protocol/merkle.hpp>

#include <hive/utilities/signal.hpp>

#include <fc/crypto/restartable_sha256.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

namespace {

// Test-only emulation of a future consumer of the state stamp mechanism. It hooks the same points
// that a real consumer (e.g. RC) would: it builds per-block rolling hashes of the account names that
// appear in operations of a given kind (articles = root comments, replies = non-root comments, votes)
// and, once HF29 is active, publishes the non-empty ones into the state_stamp_data_object at the end
// of each block, then resets - exactly the pattern the mechanism is meant to support.
struct stamp_publisher : appbase::plugin< stamp_publisher >
{
  database& _db;
  std::vector< database::signal_connection_ptr > _conns;

  fc::restartable_sha256 _articles, _replies, _votes;
  bool _has_articles = false, _has_replies = false, _has_votes = false;

  // exactly the digests published during the most recently applied block, in publication order;
  // the test uses this as an independent oracle (recomputing the merkle root itself) instead of
  // trusting the object's own finalize()
  std::vector< digest_type > _last_published;

  stamp_publisher( database& db ) : _db( db )
  {
    _conns.emplace_back( _db.add_post_apply_operation_handler(
      [this]( const operation_notification& note ) { on_operation( note ); }, *this, 0 ) );
    _conns.emplace_back( _db.add_post_apply_block_handler(
      [this]( const block_notification& note ) { on_block( note ); }, *this, 0 ) );
  }
  virtual ~stamp_publisher()
  {
    for( auto& c : _conns )
      hive::utilities::disconnect_signal( c );
  }

  static const std::string& name() { static std::string name = "stamp_publisher"; return name; }
  virtual void set_program_options( appbase::options_description&, appbase::options_description& ) override {}
  virtual void plugin_for_each_dependency( plugin_processor&& ) override {}
  virtual void plugin_initialize( const appbase::variables_map& ) override {}
  virtual void plugin_startup() override {}
  virtual void plugin_shutdown() override {}

  struct op_visitor
  {
    stamp_publisher& s;
    typedef void result_type;

    void feed( fc::restartable_sha256& h, bool& flag, const account_name_type& account ) const
    {
      std::string name = account;
      h.update( name.data(), name.size() );
      flag = true;
    }

    void operator()( const comment_operation& o ) const
    {
      if( o.parent_author.size() == 0 )
        feed( s._articles, s._has_articles, o.author );
      else
        feed( s._replies, s._has_replies, o.author );
    }
    void operator()( const vote_operation& o ) const
    {
      feed( s._votes, s._has_votes, o.voter );
    }
    template< typename T > void operator()( const T& ) const {}
  };

  void on_operation( const operation_notification& note )
  {
    if( note.virtual_op )
      return;
    // accumulate strictly once per block, during its definitive application (not while the tx sits in
    // pending, nor during the throw-away block-production pass) - mirrors how a real consumer would
    // only stamp state changes that actually made it into the applied block
    if( !( _db.is_processing_block() && !_db.is_producing_block() ) )
      return;
    note.op.visit( op_visitor{ *this } );
  }

  void on_block( const block_notification& )
  {
    _last_published.clear();
    if( _db.has_hardfork( HIVE_HARDFORK_1_29_STAMP_BLOCK_EXTENSION ) )
    {
      const auto& ssdo = _db.get_state_stamp_data();
      _db.modify( ssdo, [&]( state_stamp_data_object& o )
      {
        // publication order is articles, replies, votes - kept in _last_published so the test can
        // recompute the expected merkle root over the very same inputs in the very same order
        auto publish = [&]( const digest_type& d ) { o.publish( d ); _last_published.push_back( d ); };
        if( _has_articles ) publish( _articles.to_sha256() );
        if( _has_replies )  publish( _replies.to_sha256() );
        if( _has_votes )    publish( _votes.to_sha256() );
      } );
    }

    // start a fresh accumulation for the next block regardless of hardfork state
    _articles = fc::restartable_sha256();
    _replies  = fc::restartable_sha256();
    _votes    = fc::restartable_sha256();
    _has_articles = _has_replies = _has_votes = false;
  }
};

// Minimal stand-in consumer for the negative tests: publishes one fixed digest into the state stamp
// object exactly once (on the first block after HF29 it observes). Attaching it to only one of two
// otherwise-identical databases makes their stamped state diverge without any real activity.
struct once_stamp_publisher : appbase::plugin< once_stamp_publisher >
{
  database& _db;
  database::signal_connection_ptr _conn;
  digest_type _digest;
  bool _done = false;

  once_stamp_publisher( database& db, const digest_type& digest ) : _db( db ), _digest( digest )
  {
    _conn = _db.add_post_apply_block_handler(
      [this]( const block_notification& ) { on_block(); }, *this, 0 );
  }
  virtual ~once_stamp_publisher() { hive::utilities::disconnect_signal( _conn ); }

  static const std::string& name() { static std::string name = "once_stamp_publisher"; return name; }
  virtual void set_program_options( appbase::options_description&, appbase::options_description& ) override {}
  virtual void plugin_for_each_dependency( plugin_processor&& ) override {}
  virtual void plugin_initialize( const appbase::variables_map& ) override {}
  virtual void plugin_startup() override {}
  virtual void plugin_shutdown() override {}

  void on_block()
  {
    if( _done || !_db.has_hardfork( HIVE_HARDFORK_1_29_STAMP_BLOCK_EXTENSION ) )
      return;
    _db.modify( _db.get_state_stamp_data(), [&]( state_stamp_data_object& o ) { o.publish( _digest ); } );
    _done = true;
  }
};

// starts one hardfork before HF29 so tests can first observe pre-HF29 behavior, then activate HF29
struct state_stamp_fixture : public clean_database_fixture
{
  state_stamp_fixture() : clean_database_fixture( shared_file_size_big, HIVE_HARDFORK_1_28 ) {}
  virtual ~state_stamp_fixture() {}
};

} // namespace

BOOST_FIXTURE_TEST_SUITE( state_stamp_tests, state_stamp_fixture )

BOOST_AUTO_TEST_CASE( state_stamp_positive )
{
  try
  {
    // fixture starts at HF28; confirm no extension is produced before HF29, then activate it mid-test
    stamp_publisher publisher( *db );

    ACTORS( HIVE_asset( 100'000 ), (alice)(bob)(carol)(dave)(erin)(fred)(greg)(helen)(ivan)(jane)(kevin)(laura) );
    generate_block();

    const block_read_i& block_reader = get_chain_plugin().block_reader();

    // returns the state stamp carried by block `num`, or none if there is no such extension
    auto stamp_in_block = [&]( uint32_t num ) -> fc::optional< checksum_type >
    {
      auto fb = block_reader.get_block_by_number( num );
      BOOST_REQUIRE( fb );
      for( const auto& e : fb->get_block().extensions )
        if( e.which() == block_header_extensions::tag< state_stamp >::value )
          return e.get< state_stamp >().merkle_root;
      return fc::optional< checksum_type >();
    };

    // ---- phase 1: before HF29, activity must NOT yield any extension ----
    post_comment( "alice", "a1", "t", "b", "parent", alice_post_key );
    generate_block();
    BOOST_REQUIRE( !stamp_in_block( db->head_block_num() ).valid() );

    post_comment( "bob", "b1", "t", "b", "parent", bob_post_key );
    vote( "alice", "a1", "carol", HIVE_100_PERCENT, carol_post_key );
    generate_block();
    BOOST_REQUIRE( !stamp_in_block( db->head_block_num() ).valid() );

    // ---- activate HF29 ----
    inject_hardfork( HIVE_HARDFORK_1_29 );
    BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_29_STAMP_BLOCK_EXTENSION ) );
    BOOST_REQUIRE( db->get_state_stamp_data().is_empty() );

    // Digests published during the previous block's processing, in publication order - i.e. exactly
    // what the block we are about to produce must stamp. Captured from the observer, NOT from the state
    // stamp object, so the expected value is computed independently of the object's own finalize().
    std::vector< digest_type > expected_stamps;

    // produce a block (whose transactions were already pushed to pending) and assert its stamp equals
    // the merkle root - computed here, over the previous block's published digests in publication order -
    // that we independently expect. This catches changes to accumulation, publication order or the
    // finalize() logic (reversed vector, single element, constant, ...), which reading finalize() back
    // as the oracle would not.
    auto produce_and_check = [&]()
    {
      fc::optional< checksum_type > expected;
      if( !expected_stamps.empty() )
        expected = calculate_merkle_root( expected_stamps );

      generate_block();

      auto actual = stamp_in_block( db->head_block_num() );
      BOOST_REQUIRE_EQUAL( actual.valid(), expected.valid() );
      if( expected.valid() )
        BOOST_REQUIRE( *actual == *expected );

      // what this block just published becomes the expectation for the next one
      expected_stamps = publisher._last_published;
    };

    // ---- phase 2: after HF29, drive assorted block compositions ----

    // empty block - nothing accumulated yet, so no extension
    produce_and_check();
    BOOST_REQUIRE( db->get_state_stamp_data().is_empty() );

    // single article
    post_comment( "dave", "d1", "t", "b", "parent", dave_post_key );
    produce_and_check();

    // single article again (previous block's article should now be stamped here)
    post_comment( "erin", "e1", "t", "b", "parent", erin_post_key );
    produce_and_check();

    // empty block, but should carry the stamp of erin's article from the previous block
    produce_and_check();

    // empty block again - previous was empty, so no extension
    produce_and_check();

    // single reply + single vote in one block (two different stamps accumulate)
    post_comment_to_comment( "fred", "f1", "t", "b", "dave", "d1", fred_post_key );
    vote( "dave", "d1", "greg", HIVE_100_PERCENT, greg_post_key );
    produce_and_check();

    // all three kinds in one block
    post_comment( "helen", "h1", "t", "b", "parent", helen_post_key );
    post_comment_to_comment( "ivan", "i1", "t", "b", "erin", "e1", ivan_post_key );
    vote( "dave", "d1", "jane", HIVE_100_PERCENT, jane_post_key );
    produce_and_check();

    // two articles from different authors in one block - both feed the SAME article hash,
    // so this still produces exactly one stamp
    post_comment( "kevin", "k1", "t", "b", "parent", kevin_post_key );
    post_comment( "laura", "l1", "t", "b", "parent", laura_post_key );
    produce_and_check();

    // back to empty
    produce_and_check();

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

// Negative tests: hand-craft an otherwise-valid block whose state stamp extension disagrees with the
// node's accumulated state and confirm the node rejects it via the expected assertion. A diverged node
// (or a doctored block) getting kicked out is the whole point of the mechanism. The block is signed with
// the scheduled witness key (init key on testnet), so the ONLY reason it can fail is the state stamp
// check - and HIVE_REQUIRE_ASSERT pins down exactly which assertion fires.
struct state_stamp_hf29_fixture : public clean_database_fixture
{
  state_stamp_hf29_fixture() : clean_database_fixture() {} // default fixture is already at HF29
  virtual ~state_stamp_hf29_fixture() {}

  // builds an otherwise-valid empty block on top of the head, carrying exactly the given extensions,
  // and pushes it (propagating any exception it triggers)
  void push_empty_block_with_extensions( const block_header_extensions_type& extensions )
  {
    block_header header;
    header.previous = db->head_block_id();
    header.timestamp = db->get_slot_time( 1 );
    header.witness = db->get_scheduled_witness( 1 );
    header.transaction_merkle_root = checksum_type(); // empty block
    header.extensions = extensions;

    std::vector< std::shared_ptr< full_transaction_type > > no_transactions;
    PUSH_BLOCK( get_chain_plugin(), header, no_transactions, init_account_priv_key );
  }
};

BOOST_FIXTURE_TEST_SUITE( state_stamp_negative_tests, state_stamp_hf29_fixture )

BOOST_AUTO_TEST_CASE( reject_unexpected_stamp )
{
  try
  {
    // nothing was accumulated, so the state stamp object is empty and no block may carry a stamp
    BOOST_REQUIRE( db->get_state_stamp_data().is_empty() );

    block_header_extensions_type extensions;
    extensions.insert( block_header_extensions( state_stamp{ checksum_type::hash( std::string( "bogus" ) ) } ) );

    HIVE_REQUIRE_ASSERT( push_empty_block_with_extensions( extensions ),
      "_v._state_merkle_root == nullptr" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( reject_missing_stamp )
{
  try
  {
    // make the state stamp object non-empty, so the next block is required to carry a stamp
    once_stamp_publisher stamper( *db, digest_type::hash( std::string( "diverge" ) ) );
    generate_block();
    BOOST_REQUIRE( !db->get_state_stamp_data().is_empty() );

    // a block without the extension must be rejected as missing the required stamp
    HIVE_REQUIRE_ASSERT( push_empty_block_with_extensions( block_header_extensions_type() ),
      "_v._state_merkle_root != nullptr" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( reject_mismatched_stamp )
{
  try
  {
    once_stamp_publisher stamper( *db, digest_type::hash( std::string( "diverge" ) ) );
    generate_block();
    BOOST_REQUIRE( !db->get_state_stamp_data().is_empty() );

    // the correct stamp would be db->get_state_stamp_data().finalize(); use a different value on purpose
    checksum_type wrong = checksum_type::hash( std::string( "wrong" ) );
    BOOST_REQUIRE( wrong != db->get_state_stamp_data().finalize() );

    block_header_extensions_type extensions;
    extensions.insert( block_header_extensions( state_stamp{ wrong } ) );

    HIVE_REQUIRE_ASSERT( push_empty_block_with_extensions( extensions ),
      "*_v._state_merkle_root == expected" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

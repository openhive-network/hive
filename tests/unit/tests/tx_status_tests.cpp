#include <boost/test/unit_test.hpp>

#include <boost/scope_exit.hpp>

#include <hive/protocol/exceptions.hpp>

#include <hive/chain/database_exceptions.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;

struct expectation
{
  enum notification_type
  {
    DO_NOTHING, EXECUTE_SOFT_FAIL, EXECUTE_HARD_FAIL,
    PRE_BLOCK, POST_BLOCK, FAIL_BLOCK,
    PRE_TX, POST_TX
  };

  notification_type _type = DO_NOTHING;
  bool _is_validating_block = false;
  bool _is_producing_block = false;
  bool _is_replaying_block = false;
  bool _is_processing_block = false;
  bool _is_validating_tx = false;
  bool _is_validating_one_tx = false;
  bool _is_reapplying_tx = false;
  bool _is_reapplying_one_tx = false;
  bool _is_in_control = false;
  database::transaction_status _tx_status = database::TX_STATUS_NONE;

  static expectation inc_transaction( notification_type type )
  { return { type, false, false, false, false, true, true, false, false, true, database::TX_STATUS_UNVERIFIED }; }
  static expectation pending_transaction( notification_type type )
  { return { type, false, false, false, false, false, false, true, true, false, database::TX_STATUS_PENDING }; }
  static expectation inc_block( notification_type type )
  { return { type, true, false, false, true, true, false, false, false, false, database::TX_STATUS_INC_BLOCK }; }
  static expectation new_block( notification_type type )
  { return { type, false, true, false, true, false, false, true, false, true, database::TX_STATUS_NEW_BLOCK }; }
  static expectation old_block( notification_type type )
  { return { type, false, false, true, true, false, false, false, false, false, database::TX_STATUS_BLOCK }; }

  void verify( const database& db, notification_type inc_notification ) const
  {
    const bool _soft_ = false;
    const bool _hard_ = false;
    switch( _type )
    {
    case EXECUTE_SOFT_FAIL:
      FC_ASSERT( _soft_, "Soft fail - should be handled within signal" );
      return;
    case EXECUTE_HARD_FAIL:
      HIVE_ASSERT( _hard_, plugin_exception, "Hard fail - exception should reach outside" );
      return;
    default:
      break;
    }
    BOOST_REQUIRE_EQUAL( inc_notification, _type );
    BOOST_REQUIRE_EQUAL( db.is_validating_block(), _is_validating_block );
    BOOST_REQUIRE_EQUAL( db.is_producing_block(), _is_producing_block );
    BOOST_REQUIRE_EQUAL( db.is_replaying_block(), _is_replaying_block );
    BOOST_REQUIRE_EQUAL( db.is_processing_block(), _is_processing_block );
    BOOST_REQUIRE_EQUAL( db.is_validating_tx(), _is_validating_tx );
    BOOST_REQUIRE_EQUAL( db.is_validating_one_tx(), _is_validating_one_tx );
    BOOST_REQUIRE_EQUAL( db.is_reapplying_tx(), _is_reapplying_tx );
    BOOST_REQUIRE_EQUAL( db.is_reapplying_one_tx(), _is_reapplying_one_tx );
    BOOST_REQUIRE_EQUAL( db.is_in_control(), _is_in_control );
    BOOST_REQUIRE_EQUAL( db.get_tx_status(), _tx_status );
  }
};

struct expectation_set : appbase::plugin< expectation_set >
{
  database& _db;
  std::list< expectation > current_expectations;
  std::vector< boost::signals2::connection > connections;
  bool failure = false;

  expectation_set( database& db ) : _db( db )
  {
    connections.emplace_back( _db.add_pre_apply_block_handler(
      [this]( const block_notification& ){ verify( expectation::PRE_BLOCK); }, *this, 0 ) );
    connections.emplace_back( _db.add_post_apply_block_handler(
      [this]( const block_notification& ) { verify( expectation::POST_BLOCK ); }, *this, 0 ) );
    connections.emplace_back( _db.add_fail_apply_block_handler(
      [this]( const block_notification& ) { verify( expectation::FAIL_BLOCK ); }, *this, 0 ) );
    connections.emplace_back( _db.add_pre_apply_transaction_handler(
      [this]( const transaction_notification& ) { verify( expectation::PRE_TX ); }, *this, 0 ) );
    connections.emplace_back( _db.add_post_apply_transaction_handler(
      [this]( const transaction_notification& ) { verify( expectation::POST_TX ); }, *this, 0 ) );
  }
  virtual ~expectation_set()
  {
    for( auto& connection : connections )
      chain::util::disconnect_signal( connection );
  }

  void expect( expectation&& x ) { current_expectations.emplace_back( std::move( x ) ); }
  void reset_expectations() { current_expectations.clear(); }
  void verify( expectation::notification_type notification )
  {
    bool expect_exception = false;
    try
    {
      BOOST_REQUIRE( !failure && !current_expectations.empty() );
      expectation expected( std::move( current_expectations.front() ) );
      current_expectations.pop_front();
      expect_exception = expected._type == expectation::EXECUTE_SOFT_FAIL || expected._type == expectation::EXECUTE_HARD_FAIL;
      expected.verify( _db, notification );
    }
    catch( const fc::exception& ex )
    {
      if( expect_exception )
        throw;
      const bool _fc_exception_ = false;
      failure = true; //if exception happens during pending tx processing, it will be silently handled
      HIVE_ASSERT( _fc_exception_, plugin_exception, "Unexpected fc::exception ${e}", ( "e", ex ) );
    }
    catch( ... )
    {
      const bool _any_exception_ = false;
      failure = true; //if exception happens during pending tx processing, it will be silently handled
      HIVE_ASSERT( _any_exception_, plugin_exception, "Unknown exception (most likely BOOST_REQUIRE fail" );
    }
  }
  void check_empty() const
  {
    BOOST_REQUIRE( !failure && current_expectations.empty() );
    expectation empty;
    empty.verify( _db, expectation::DO_NOTHING );
  }

  static const std::string& name() { static std::string name = "test"; return name; }
private: //just because it is (almost unused) part of signal registration
  virtual void set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) override {}
  virtual void plugin_for_each_dependency( plugin_processor&& processor ) override {}
  virtual void plugin_initialize( const appbase::variables_map& options ) override {}
  virtual void plugin_startup() override {}
  virtual void plugin_shutdown() override {}
};

BOOST_FIXTURE_TEST_SUITE( tx_status_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( regular_transactions )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing pushing regular transactions" );

    db_plugin->debug_update( [&]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& dgpo )
      {
        dgpo.maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT;
        dgpo.required_actions_partition_percent = 0;
      } );
    } );

    ACTORS( (alice)(bob)(carol)(dan) )
    generate_block();

    expectation_set check( *db );

    BOOST_TEST_MESSAGE( "Valid transaction" );
    comment_operation comment;
    comment.parent_permlink = "txstatus";
    comment.author = "alice";
    comment.permlink = "test";
    comment.title = "no title";
    comment.body = "empty";
    check.expect( expectation::inc_transaction( expectation::PRE_TX ) );
    check.expect( expectation::inc_transaction( expectation::POST_TX ) );
    push_transaction( comment, alice_private_key );
    check.check_empty();

    BOOST_TEST_MESSAGE( "Failed transaction" );
    comment.parent_author = "alice";
    check.expect( expectation::inc_transaction( expectation::PRE_TX ) );
    HIVE_REQUIRE_CHAINBASE_ASSERT( push_transaction( comment, alice_private_key ), "unknown key" );
    check.check_empty();

    BOOST_TEST_MESSAGE( "Generating first block after transaction" );
    //expect just one transaction (note that new block has no pre/post block notifications)
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    //block is reapplied right after it is produced
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    generate_block();
    check.check_empty();

    BOOST_TEST_MESSAGE( "Large transactions - only one fits block" );
    comment.parent_author = "alice";
    comment.parent_permlink = "test";
    comment.author = "bob";
    const std::string BODY_ELEMENT( "aaaaabbbbbcccccdddddeeeeefffffggggghhhhhiiiiijjjjj" );
    const int BODY_ELEMENTS = 1300;
    comment.body = "";
    comment.body.reserve( BODY_ELEMENT.size() * BODY_ELEMENTS );
    for( int i = 0; i < BODY_ELEMENTS; ++i )
      comment.body += BODY_ELEMENT;
    check.expect( expectation::inc_transaction( expectation::PRE_TX ) );
    check.expect( expectation::inc_transaction( expectation::POST_TX ) );
    push_transaction( comment, bob_private_key );
    check.check_empty();
    comment.author = "carol";
    check.expect( expectation::inc_transaction( expectation::PRE_TX ) );
    check.expect( expectation::inc_transaction( expectation::POST_TX ) );
    push_transaction( comment, carol_private_key );
    check.check_empty();
    comment.author = "dan";
    check.expect( expectation::inc_transaction( expectation::PRE_TX ) );
    check.expect( expectation::inc_transaction( expectation::POST_TX ) );
    push_transaction( comment, dan_private_key );
    check.check_empty();

    BOOST_TEST_MESSAGE( "Generating block with 1 of 3 large transactions" );
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    //remaining transactions are reapplied as pending
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    check.expect( expectation::pending_transaction( expectation::POST_TX ) );
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    check.expect( expectation::pending_transaction( expectation::POST_TX ) );
    generate_block();
    check.check_empty();

    BOOST_TEST_MESSAGE( "Generating block with 2 of 3 large transactions" );
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    //remaining transaction is reapplied as pending
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    check.expect( expectation::pending_transaction( expectation::POST_TX ) );
    generate_block();
    check.check_empty();

    BOOST_TEST_MESSAGE( "Generating block with last of large transactions" );
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    generate_block();
    check.check_empty();

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( popped_transactions )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing popped transactions" );

    ACTORS( (alice)(bob)(carol) )
    generate_block();
    fund( "alice", 10000 );
    fund( "bob", 10000 );
    fund( "carol", 10000 );
    generate_block();

    expectation_set check( *db );

    BOOST_TEST_MESSAGE( "Valid transactions" );
    transfer_operation transfer;
    transfer.from = "alice";
    transfer.to = "bob";
    transfer.amount = ASSET( "10.000 TESTS" );
    check.expect( expectation::inc_transaction( expectation::PRE_TX ) );
    check.expect( expectation::inc_transaction( expectation::POST_TX ) );
    push_transaction( transfer, alice_private_key );
    check.check_empty();
    transfer.from = "bob";
    transfer.to = "carol";
    check.expect( expectation::inc_transaction( expectation::PRE_TX ) );
    check.expect( expectation::inc_transaction( expectation::POST_TX ) );
    push_transaction( transfer, bob_private_key );
    check.check_empty();
    transfer.from = "carol";
    transfer.to = "alice";
    check.expect( expectation::inc_transaction( expectation::PRE_TX ) );
    check.expect( expectation::inc_transaction( expectation::POST_TX ) );
    push_transaction( transfer, carol_private_key );
    check.check_empty();

    BOOST_TEST_MESSAGE( "Generating first block after transactions" );
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    generate_block();
    check.check_empty();

    BOOST_TEST_MESSAGE( "Popping generated block - all included transactions become popped" );
    db->pop_block();

    BOOST_TEST_MESSAGE( "Generating empty block and converting popped transactions to pending" );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    check.expect( expectation::pending_transaction( expectation::POST_TX ) );
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    check.expect( expectation::pending_transaction( expectation::POST_TX ) );
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    check.expect( expectation::pending_transaction( expectation::POST_TX ) );
    generate_block();
    check.check_empty();

    BOOST_TEST_MESSAGE( "Generating block out of previously popped (now pending) transactions" );
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    generate_block();
    check.check_empty();

    BOOST_TEST_MESSAGE( "Popping generated block again" );
    db->pop_block();

    BOOST_TEST_MESSAGE( "Blocking one of popped transfers with another one that will be applied first" );
    transfer.from = "alice";
    transfer.to = "bob";
    transfer.amount = ASSET( "0.010 TESTS" );
    check.expect( expectation::inc_transaction( expectation::PRE_TX ) );
    check.expect( expectation::inc_transaction( expectation::POST_TX ) );
    push_transaction( transfer, alice_private_key );
    check.check_empty();

    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) ); //that tx (alice->bob transfer) fails
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    check.expect( expectation::pending_transaction( expectation::POST_TX ) );
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    check.expect( expectation::pending_transaction( expectation::POST_TX ) );
    generate_block();
    check.check_empty();
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == 19990 );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == 10 );
    BOOST_REQUIRE( get_balance( "carol" ).amount.value == 10000 );

    BOOST_TEST_MESSAGE( "Generating block out of remaining previously popped transactions" );
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    generate_block();
    check.check_empty();

    BOOST_TEST_MESSAGE( "Popping generated block one more time" );
    auto block = db->fetch_block_by_number( db->head_block_num() );
    BOOST_REQUIRE(block);
    db->pop_block();

    BOOST_TEST_MESSAGE( "Pushing block as if it came from P2P/API" );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    //popped transactions are dropped silently as duplicates of those that came with block
    db->push_block(block);
    check.check_empty();

    BOOST_TEST_MESSAGE( "Generating empty block - no popped/pending transactions remain" );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    generate_block();
    check.check_empty();

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transactions_in_forks )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing transactions in forked database" );

    ACTORS( (alice)(bob)(carol) )
    generate_block();
    fund( "alice", 10000 );
    fund( "bob", 10000 );
    fund( "carol", 10000 );
    generate_block();

    const auto& dgpo = db->get_dynamic_global_properties();

    BOOST_TEST_MESSAGE( "Build first fork" );
    std::vector< std::shared_ptr<full_block_type> > reality1;

    auto push = [&]( const operation& op, const fc::ecc::private_key& key )
    {
      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION/2 );
        //if we used max expiration time, popped transactions would fail not due to tapos, but because
        //their expiration time is too much into the future
      tx.set_reference_block( dgpo.head_block_id );
        //normal database_fixture::push_transaction() does not set tapos - they all point to genesis
      tx.operations.push_back( op );
      sign( tx, key );
      push_transaction( tx, 0 );
    };

    transfer_operation transfer;
    transfer.from = "alice";
    transfer.to = "bob";
    transfer.amount = ASSET( "0.001 TESTS" );
    transfer.memo = "Hello there";
    push( transfer, alice_private_key );
    generate_block();
    transfer.memo = "I haven't seen you in a while";
    push( transfer, alice_private_key );
    generate_block();
    transfer.memo = "Maybe we should meet sometimes";
    push( transfer, alice_private_key );
    generate_block();
    generate_block(); //common block of both realities
    generate_block(); //0
    transfer.from = "bob";
    transfer.to = "alice";
    transfer.memo = "Sure";
    push( transfer, bob_private_key );
    generate_block(); //1
    transfer.memo = "I'm usually free on Mondays";
    push( transfer, bob_private_key );
    generate_block(); //2
    transfer.memo = "If it's ok with you let's meet at our old cafe in three days. They still offer those sweet bagels you liked so much";
    push( transfer, bob_private_key );
    generate_block(); //3
    generate_block(); //4
    generate_block(); //5
    transfer.from = "carol";
    transfer.to = "bob";
    transfer.memo = "DID YOU THINK I WON'T NOTICE, YOU CHEATING @#$%?!";
    push( transfer, carol_private_key );
    generate_block(); //6
    transfer.to = "alice";
    transfer.memo = "YOU SEDUCTIVE VIXEN! I told you before - leave my man alone!";
    push( transfer, carol_private_key );
    generate_block(); //7
    generate_block(); //8
    generate_block(); //9

    BOOST_TEST_MESSAGE( "Rewind up to first answer from bob" );
    reality1.resize( 10 );
    for( int i = 10; i > 0; )
    {
      --i;
      reality1[i] = db->fetch_block_by_number( db->head_block_num() );
      db->pop_block();
    }

    BOOST_TEST_MESSAGE( "Build alternative reality" );
    {
      expectation_set check( *db );

      //we have to add something to it, or new incarnation of block 0 would also be empty like original
      //and would have the same id, which means tapos would not fail for first popped transaction
      transfer.from = "initminer";
      transfer.to = "null";
      transfer.memo = "time travel fee";
      check.expect( expectation::inc_transaction( expectation::PRE_TX ) );
      check.expect( expectation::inc_transaction( expectation::POST_TX ) );
      push( transfer, init_account_priv_key );
      check.check_empty();

      check.expect( expectation::new_block( expectation::PRE_TX ) );
      check.expect( expectation::new_block( expectation::POST_TX ) );
      check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
      check.expect( expectation::inc_block( expectation::PRE_TX ) );
      check.expect( expectation::inc_block( expectation::POST_TX ) );
      check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
      //transactions popped from block 1/2/3/6/7 all fail on tapos check (which happens prior to notification)
      generate_block(); //0'
      check.check_empty();
    }

    transfer.from = "bob";
    transfer.to = "alice";
    transfer.memo = "I'm sorry, but Carol is really jealous of you. She is a good girl but can be a pain at times";
    push( transfer, bob_private_key );
    generate_block(); //1'
    transfer.memo = "She'd kill me and possibly you and herself if we met";
    push( transfer, bob_private_key );
    generate_block(); //2'
    transfer.memo = "Did you know she still calls you 'Boobasaur'?";
    push( transfer, bob_private_key );
    generate_block(); //3'
    generate_block(); //4'
    transfer.from = "alice";
    transfer.to = "bob";
    transfer.memo = "Oh, I thought you broke up with that lunatic, never mind then";
    push( transfer, alice_private_key );
    transfer.memo = "I bet she is standing behind you right now telling you what to write";
    push( transfer, alice_private_key );
    generate_block(); //5'
    transfer.from = "carol";
    transfer.to = "alice";
    transfer.memo = "I AM NOT!";
    push( transfer, carol_private_key );
    generate_block(); //6'
    generate_block(); //7'
    transfer.from = "alice";
    transfer.to = "carol";
    transfer.memo = "lol";
    push( transfer, alice_private_key );
    generate_block(); //8'

    expectation_set check( *db );

    BOOST_TEST_MESSAGE( "Reapply previous reality on top of current one" );
    //blocks are not even validated until fork becomes longer than previous one
    for( int i = 0; i < 9; ++i )
    {
      auto block = reality1[i];
      db->push_block(block);
    }
    //then all blocks from fork are applied one by one
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //0
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //1
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //2
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //3
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //4
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //5
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //6
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //7
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //8
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //9
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    //transactions from second reality got popped and fail on tapos during reapplication
    //all except initminer paying for time travel
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    check.expect( expectation::pending_transaction( expectation::POST_TX ) );
    {
      auto block = reality1[9];
      push_block(block);
    }
    check.check_empty();

    //time travel fee transaction became pending above, now it is included in block
    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    generate_block();
    check.check_empty();

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( failure_during_fork_switch )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing transactions during failed for switch" );

    ACTORS( (alice)(bob)(carol) )
    generate_block();
    fund( "alice", 10000 );
    generate_block();

    BOOST_TEST_MESSAGE( "Build fork" );
    std::vector< std::shared_ptr<full_block_type> > reality1;

    transfer_operation transfer;
    transfer.from = "alice";
    transfer.to = "bob";
    transfer.amount = ASSET( "10.000 TESTS" );
    push_transaction( transfer, alice_private_key );
    generate_block(); //0
    transfer.from = "bob";
    transfer.to = "carol";
    transfer.amount = ASSET( "10.000 TESTS" );
    push_transaction( transfer, bob_private_key );
    generate_block(); //1
    transfer.from = "carol";
    transfer.to = "alice";
    transfer.amount = ASSET( "10.000 TESTS" );
    push_transaction( transfer, carol_private_key );
    generate_block(); //2

    BOOST_TEST_MESSAGE( "Rewind last three blocks" );
    reality1.resize( 3 );
    for( int i = 3; i > 0; )
    {
      --i;
      reality1[i] = db->fetch_block_by_number( db->head_block_num() );
      db->pop_block();
    }

    expectation_set check( *db );

    BOOST_TEST_MESSAGE( "Build alternative reality" );
    transfer.from = "alice";
    transfer.to = "bob";
    transfer.amount = ASSET( "0.001 TESTS" );
    check.expect( expectation::inc_transaction( expectation::PRE_TX ) );
    check.expect( expectation::inc_transaction( expectation::POST_TX ) );
    push_transaction( transfer, alice_private_key );
    check.check_empty();

    check.expect( expectation::new_block( expectation::PRE_TX ) );
    check.expect( expectation::new_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    //previously popped transactions will try to be reapplied, but fail due to not enough balance,
    //since new reality contains small transfer from alice, making further transfers too big
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    //last transaction (from block 2) has its expiration too far into the future, so it fails prior to notification
    generate_block(); //0'
    check.check_empty();

    BOOST_TEST_MESSAGE( "Reapply original reality" );
    //new reality contains one block, so we need to push two to make it longer
    {
      auto block = reality1[0];
      push_block( block );
    }
    //fork will be switched now - let's force failure during reapplication of block 1
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //0
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //1
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::EXECUTE_HARD_FAIL ) );
    check.expect( expectation::inc_block( expectation::FAIL_BLOCK ) );
    //now the mechanism should pop transaction from block 0 (block 1 is dropped as failing),
    //reapply block 0' ...
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) ); //0'
    check.expect( expectation::inc_block( expectation::PRE_TX ) );
    check.expect( expectation::inc_block( expectation::POST_TX ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    //...and then try to reapply popped transaction which will fail on insufficient balance
    //like it did before
    check.expect( expectation::pending_transaction( expectation::PRE_TX ) );
    {
      auto block = reality1[1];
      BOOST_REQUIRE_THROW( push_block( block ), plugin_exception );
        //unfortunately fc::last_assert_expression is overwritten by exception from failed popped
        //transaction, so we can't use HIVE_REQUIRE_EXCEPTION( db->push_block( *block ), "_hard_", plugin_exception );
    }
    check.check_empty();
    //pushing last block from fork is irrelevant as it will be dropped immediately
    //since fork it linked to was also dropped
    {
      auto block = reality1[2];
      HIVE_REQUIRE_EXCEPTION( push_block( block ), "itr != index.end()", unlinkable_block_exception );
    }

    //generate one more block to verify that we have no pending transactions
    check.expect( expectation::inc_block( expectation::PRE_BLOCK ) );
    check.expect( expectation::inc_block( expectation::POST_BLOCK ) );
    generate_block();
    check.check_empty();

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_SUITE_END()

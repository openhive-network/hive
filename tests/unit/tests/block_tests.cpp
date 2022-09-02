/*
  * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
  *
  * The MIT License
  *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in
  * all copies or substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  * THE SOFTWARE.
  */
#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>
#include <hive/plugins/witness/block_producer.hpp>

#include <hive/utilities/tempdir.hpp>
#include <hive/utilities/database_configuration.hpp>

#include <fc/crypto/digest.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins;

#define TEST_SHARED_MEM_SIZE (1024 * 1024 * 8)

BOOST_AUTO_TEST_SUITE(block_tests)

void open_test_database( database& db, const fc::path& dir )
{
  hive::chain::open_args args;
  args.data_dir = dir;
  args.shared_mem_dir = dir;
  args.initial_supply = INITIAL_TEST_SUPPLY;
  args.hbd_initial_supply = HBD_INITIAL_TEST_SUPPLY;
  args.shared_file_size = TEST_SHARED_MEM_SIZE;
  args.database_cfg = hive::utilities::default_database_configuration();
  db.open( args );
}

BOOST_AUTO_TEST_CASE( generate_empty_blocks )
{
  try {
    fc::time_point_sec now( HIVE_TESTING_GENESIS_TIMESTAMP );
    fc::temp_directory data_dir( hive::utilities::temp_directory_path() );
    std::shared_ptr<full_block_type> b;

    // TODO:  Don't generate this here
    auto init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash( string( "init_key" ) ) );
    std::shared_ptr<full_block_type> cutoff_block;
    {
      database db;
      witness::block_producer bp( db );
      db._log_hardforks = false;
      open_test_database( db, data_dir.path() );
      b = GENERATE_BLOCK( bp, db.get_slot_time(1), db.get_scheduled_witness(1),
        init_account_priv_key, database::skip_nothing );

      // TODO:  Change this test when we correct #406
      // n.b. we generate HIVE_MIN_UNDO_HISTORY+1 extra blocks which will be discarded on save
      for( uint32_t i = 1; ; ++i )
      {
        BOOST_CHECK( db.head_block_id() == b->get_block_id() );
        string cur_witness = db.get_scheduled_witness(1);
        b = GENERATE_BLOCK( bp, db.get_slot_time(1), cur_witness,
          init_account_priv_key, database::skip_nothing );
        BOOST_CHECK( b->get_block_header().witness == cur_witness );
        uint32_t cutoff_height = db.get_last_irreversible_block_num();
        if( cutoff_height >= 200 )
        {
          auto block = db.fetch_block_by_number( cutoff_height );
          BOOST_REQUIRE( block );
          cutoff_block = block;
          break;
        }
      }
      db.close();
    }
    {
      database db;
      witness::block_producer bp( db );
      db._log_hardforks = false;
      open_test_database( db, data_dir.path() );

      BOOST_CHECK_EQUAL( db.head_block_num(), cutoff_block->get_block_num() );

      b = cutoff_block;
      for( uint32_t i = 0; i < 200; ++i )
      {
        BOOST_CHECK( db.head_block_id() == b->get_block_id() );

        string cur_witness = db.get_scheduled_witness(1);
        b = GENERATE_BLOCK( bp, db.get_slot_time(1), cur_witness,
          init_account_priv_key, database::skip_nothing );
      }
      BOOST_CHECK_EQUAL( db.head_block_num(), cutoff_block->get_block_num()+200 );
    }
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( undo_block )
{
  try {
    fc::temp_directory data_dir( hive::utilities::temp_directory_path() );
    {
      database db;
      witness::block_producer bp( db );
      db._log_hardforks = false;
      open_test_database( db, data_dir.path() );
      fc::time_point_sec now( HIVE_TESTING_GENESIS_TIMESTAMP );
      std::vector< time_point_sec > time_stack;

      auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")) );
      for( uint32_t i = 0; i < 5; ++i )
      {
        now = db.get_slot_time(1);
        time_stack.push_back( now );
        GENERATE_BLOCK( bp, now, db.get_scheduled_witness(1),
          init_account_priv_key, database::skip_nothing );
      }
      BOOST_CHECK( db.head_block_num() == 5 );
      BOOST_CHECK( db.head_block_time() == now );
      db.pop_block();
      time_stack.pop_back();
      now = time_stack.back();
      BOOST_CHECK( db.head_block_num() == 4 );
      BOOST_CHECK( db.head_block_time() == now );
      db.pop_block();
      time_stack.pop_back();
      now = time_stack.back();
      BOOST_CHECK( db.head_block_num() == 3 );
      BOOST_CHECK( db.head_block_time() == now );
      db.pop_block();
      time_stack.pop_back();
      now = time_stack.back();
      BOOST_CHECK( db.head_block_num() == 2 );
      BOOST_CHECK( db.head_block_time() == now );
      for( uint32_t i = 0; i < 5; ++i )
      {
        now = db.get_slot_time(1);
        time_stack.push_back( now );
        auto b = GENERATE_BLOCK( bp, now, db.get_scheduled_witness(1),
          init_account_priv_key, database::skip_nothing );
      }
      BOOST_CHECK( db.head_block_num() == 7 );
    }
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( fork_blocks )
{
  try {
    fc::temp_directory data_dir1( hive::utilities::temp_directory_path() );
    fc::temp_directory data_dir2( hive::utilities::temp_directory_path() );

    //TODO This test needs 6-7 ish witnesses prior to fork

    database db1;
    witness::block_producer bp1( db1 );
    db1._log_hardforks = false;
    open_test_database( db1, data_dir1.path() );
    database db2;
    witness::block_producer bp2( db2 );
    db2._log_hardforks = false;
    open_test_database( db2, data_dir2.path() );

    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")) );
    for( uint32_t i = 0; i < 10; ++i )
    {
      auto b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
        init_account_priv_key, database::skip_nothing );
      try {
        PUSH_BLOCK( db2, b );
      } FC_CAPTURE_AND_RETHROW( ("db2") );
    }
    for( uint32_t i = 10; i < 13; ++i )
    {
      auto b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
        init_account_priv_key, database::skip_nothing );
    }
    string db1_tip = db1.head_block_id().str();
    uint32_t next_slot = 3;
    for( uint32_t i = 13; i < 16; ++i )
    {
      auto b = GENERATE_BLOCK( bp2, db2.get_slot_time(next_slot), db2.get_scheduled_witness(next_slot),
        init_account_priv_key, database::skip_nothing );
      next_slot = 1;
      // notify both databases of the new block.
      // only db2 should switch to the new fork, db1 should not
      PUSH_BLOCK( db1, b );
      BOOST_CHECK_EQUAL(db1.head_block_id().str(), db1_tip);
      BOOST_CHECK_EQUAL(db2.head_block_id().str(), b->get_block_id().str());
    }

    //The two databases are on distinct forks now, but at the same height. Make a block on db2, make it invalid, then
    //pass it to db1 and assert that db1 doesn't switch to the new fork.
    std::shared_ptr<full_block_type> good_block;
    BOOST_CHECK_EQUAL(db1.head_block_num(), 13u);
    BOOST_CHECK_EQUAL(db2.head_block_num(), 13u);
    {
      auto b = GENERATE_BLOCK( bp2, db2.get_slot_time(1), db2.get_scheduled_witness(1),
        init_account_priv_key, database::skip_nothing );
      good_block = b;
      signed_block bad_block = b->get_block();
      bad_block.transactions.emplace_back(signed_transaction());
      bad_block.transactions.back().operations.emplace_back(transfer_operation());
      bad_block.legacy_sign( init_account_priv_key );
      BOOST_CHECK_EQUAL(bad_block.block_num(), 14u);
      HIVE_CHECK_THROW(PUSH_BLOCK( db1, bad_block ), fc::exception);
    }
    BOOST_CHECK_EQUAL(db1.head_block_num(), 13u);
    BOOST_CHECK_EQUAL(db1.head_block_id().str(), db1_tip);

    // assert that db1 switches to new fork with good block
    BOOST_CHECK_EQUAL(db2.head_block_num(), 14u);
    PUSH_BLOCK( db1, good_block );
    BOOST_CHECK_EQUAL(db1.head_block_id().str(), db2.head_block_id().str());
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( switch_forks_undo_create )
{
  try {
    fc::temp_directory dir1( hive::utilities::temp_directory_path() ),
                  dir2( hive::utilities::temp_directory_path() );
    database db1,
          db2;
    witness::block_producer bp1( db1 ),
                    bp2( db2 );
    db1._log_hardforks = false;
    open_test_database( db1, dir1.path() );
    db2._log_hardforks = false;
    open_test_database( db2, dir2.path() );

    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")) );
    public_key_type init_account_pub_key  = init_account_priv_key.get_public_key();
    db1.get_index< account_index >();

    //*
    signed_transaction trx;
    account_create_operation cop;
    cop.new_account_name = "alice";
    cop.creator = HIVE_INIT_MINER_NAME;
    cop.owner = authority(1, init_account_pub_key, 1);
    cop.active = cop.owner;
    trx.operations.push_back(cop);
    trx.set_expiration( db1.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    trx.sign( init_account_priv_key, db1.get_chain_id(), fc::ecc::fc_canonical );
    PUSH_TX( db1, trx );
    //*/
    // generate blocks
    // db1 : A
    // db2 : B C D

    auto b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );

    auto alice_id = db1.get_account( "alice" ).get_id();
    BOOST_CHECK( db1.get(alice_id).name == "alice" );

    b = GENERATE_BLOCK( bp2, db2.get_slot_time(1), db2.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    PUSH_BLOCK( db1, b );
    b = GENERATE_BLOCK( bp2, db2.get_slot_time(1), db2.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    PUSH_BLOCK( db1, b );
    HIVE_REQUIRE_THROW(db2.get(alice_id), std::exception);
    db1.get(alice_id); /// it should be included in the pending state
    db1.clear_pending(); // clear it so that we can verify it was properly removed from pending state.
    HIVE_REQUIRE_THROW(db1.get(alice_id), std::exception);

    PUSH_TX( db2, trx );

    b = GENERATE_BLOCK( bp2, db2.get_slot_time(1), db2.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    PUSH_BLOCK( db1, b );

    BOOST_CHECK( db1.get(alice_id).name == "alice");
    BOOST_CHECK( db2.get(alice_id).name == "alice");
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( duplicate_transactions )
{
  try {
    fc::temp_directory dir1( hive::utilities::temp_directory_path() ),
                  dir2( hive::utilities::temp_directory_path() );
    database db1,
          db2;
    witness::block_producer bp1( db1 );
    db1._log_hardforks = false;
    open_test_database( db1, dir1.path() );
    db2._log_hardforks = false;
    open_test_database( db2, dir2.path() );
    BOOST_CHECK( db1.get_chain_id() == db2.get_chain_id() );

    auto skip_sigs = database::skip_transaction_signatures | database::skip_authority_check;

    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")) );
    public_key_type init_account_pub_key  = init_account_priv_key.get_public_key();

    signed_transaction trx;
    account_create_operation cop;
    cop.new_account_name = "alice";
    cop.creator = HIVE_INIT_MINER_NAME;
    cop.owner = authority(1, init_account_pub_key, 1);
    cop.active = cop.owner;
    trx.operations.push_back(cop);
    trx.set_expiration( db1.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    trx.sign( init_account_priv_key, db1.get_chain_id(), fc::ecc::fc_canonical );
    PUSH_TX( db1, trx, skip_sigs );

    trx = decltype(trx)();
    transfer_operation t;
    t.from = HIVE_INIT_MINER_NAME;
    t.to = "alice";
    t.amount = asset(500,HIVE_SYMBOL);
    trx.operations.push_back(t);
    trx.set_expiration( db1.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    trx.sign( init_account_priv_key, db1.get_chain_id(), fc::ecc::fc_canonical );
    PUSH_TX( db1, trx, skip_sigs );

    HIVE_CHECK_THROW(PUSH_TX( db1, trx, skip_sigs ), fc::exception);

    auto b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
      init_account_priv_key, skip_sigs );
    PUSH_BLOCK( db2, b, skip_sigs );

    HIVE_CHECK_THROW(PUSH_TX( db1, trx, skip_sigs ), fc::exception);
    HIVE_CHECK_THROW(PUSH_TX( db2, trx, skip_sigs ), fc::exception);
    BOOST_CHECK_EQUAL(db1.get_balance( "alice", HIVE_SYMBOL ).amount.value, 500);
    BOOST_CHECK_EQUAL(db2.get_balance( "alice", HIVE_SYMBOL ).amount.value, 500);
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( tapos )
{
  try {
    fc::temp_directory dir1( hive::utilities::temp_directory_path() );
    database db1;
    witness::block_producer bp1( db1 );
    db1._log_hardforks = false;
    open_test_database( db1, dir1.path() );

    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")) );
    public_key_type init_account_pub_key  = init_account_priv_key.get_public_key();

    auto b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );

    BOOST_TEST_MESSAGE( "Creating a transaction with reference block" );
    idump((db1.head_block_id()));
    signed_transaction trx;
    //This transaction must be in the next block after its reference, or it is invalid.
    trx.set_reference_block( db1.head_block_id() );

    account_create_operation cop;
    cop.new_account_name = "alice";
    cop.creator = HIVE_INIT_MINER_NAME;
    cop.owner = authority(1, init_account_pub_key, 1);
    cop.active = cop.owner;
    trx.operations.push_back(cop);
    trx.set_expiration( db1.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    trx.sign( init_account_priv_key, db1.get_chain_id(), fc::ecc::fc_canonical );

    BOOST_TEST_MESSAGE( "Pushing Pending Transaction" );
    idump((trx));
    PUSH_TX(db1, trx);
    BOOST_TEST_MESSAGE( "Generating a block" );
    b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    trx.clear();

    transfer_operation t;
    t.from = HIVE_INIT_MINER_NAME;
    t.to = "alice";
    t.amount = asset(50,HIVE_SYMBOL);
    trx.operations.push_back(t);
    trx.set_expiration( db1.head_block_time() + fc::seconds(2) );
    trx.sign( init_account_priv_key, db1.get_chain_id(), fc::ecc::fc_canonical );
    idump((trx)(db1.head_block_time()));
    b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    idump((b->get_block()));
    b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    trx.signatures.clear();
    trx.sign( init_account_priv_key, db1.get_chain_id(), fc::ecc::fc_canonical );
    BOOST_REQUIRE_THROW( PUSH_TX(db1, trx, 0/*database::skip_transaction_signatures | database::skip_authority_check*/), fc::exception );
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_FIXTURE_TEST_CASE( optional_tapos, clean_database_fixture )
{
  try
  {
    idump((db->get_account("initminer")));
    ACTORS( (alice)(bob) );

    generate_block();

    BOOST_TEST_MESSAGE( "Create transaction" );

    transfer( HIVE_INIT_MINER_NAME, "alice", asset( 1000000, HIVE_SYMBOL ) );
    transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.amount = asset(1000,HIVE_SYMBOL);
    signed_transaction tx;
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "ref_block_num=0, ref_block_prefix=0" );

    tx.ref_block_num = 0;
    tx.ref_block_prefix = 0;
    tx.signatures.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    PUSH_TX( *db, tx );

    BOOST_TEST_MESSAGE( "proper ref_block_num, ref_block_prefix" );

    tx.signatures.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    PUSH_TX( *db, tx, database::skip_transaction_dupe_check );

    BOOST_TEST_MESSAGE( "ref_block_num=0, ref_block_prefix=12345678" );

    tx.ref_block_num = 0;
    tx.ref_block_prefix = 0x12345678;
    tx.signatures.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( PUSH_TX( *db, tx, database::skip_transaction_dupe_check ), fc::exception );

    BOOST_TEST_MESSAGE( "ref_block_num=1, ref_block_prefix=12345678" );

    tx.ref_block_num = 1;
    tx.ref_block_prefix = 0x12345678;
    tx.signatures.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( PUSH_TX( *db, tx, database::skip_transaction_dupe_check ), fc::exception );

    BOOST_TEST_MESSAGE( "ref_block_num=9999, ref_block_prefix=12345678" );

    tx.ref_block_num = 9999;
    tx.ref_block_prefix = 0x12345678;
    tx.signatures.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( PUSH_TX( *db, tx, database::skip_transaction_dupe_check ), fc::exception );
  }
  catch (fc::exception& e)
  {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_FIXTURE_TEST_CASE( double_sign_check, clean_database_fixture )
{ try {
  generate_block();
  ACTOR(bob);
  share_type amount = 1000;

  transfer_operation t;
  t.from = HIVE_INIT_MINER_NAME;
  t.to = "bob";
  t.amount = asset(amount,HIVE_SYMBOL);
  trx.operations.push_back(t);
  trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  trx.validate();

  push_transaction(trx, ~0);

  trx.operations.clear();
  t.from = "bob";
  t.to = HIVE_INIT_MINER_NAME;
  t.amount = asset(amount,HIVE_SYMBOL);
  trx.operations.push_back(t);
  trx.validate();

  BOOST_TEST_MESSAGE( "Verify that not-signing causes an exception" );
  HIVE_REQUIRE_THROW( push_transaction(trx, 0), fc::exception );

  BOOST_TEST_MESSAGE( "Verify that double-signing causes an exception" );
  sign( trx, bob_private_key );
  sign( trx, bob_private_key );
  HIVE_REQUIRE_THROW( push_transaction(trx, 0), tx_duplicate_sig );

  BOOST_TEST_MESSAGE( "Verify that signing with an extra, unused key fails" );
  trx.signatures.pop_back();
  sign( trx, generate_private_key( "bogus" ) );
  HIVE_REQUIRE_THROW( push_transaction(trx, 0), tx_irrelevant_sig );

  BOOST_TEST_MESSAGE( "Verify that signing once with the proper key passes" );
  trx.signatures.pop_back();
  push_transaction(trx, 0);
  sign( trx, bob_private_key );

} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE( pop_block_twice, clean_database_fixture )
{
  try
  {
    uint32_t skip_flags = (
        database::skip_witness_signature
      | database::skip_transaction_signatures
      | database::skip_authority_check
      );

    // Sam is the creator of accounts
    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")) );
    private_key_type sam_key = generate_private_key( "sam" );
    account_create( "sam", sam_key.get_public_key() );

    //Get a sane head block time
    generate_block( skip_flags );

    transaction tx;
    signed_transaction ptx;

    db->get_account( HIVE_INIT_MINER_NAME );
    // transfer from committee account to Sam account
    transfer( HIVE_INIT_MINER_NAME, "sam", asset( 100000, HIVE_SYMBOL ) );

    generate_block(skip_flags);

    account_create( "alice", generate_private_key( "alice" ).get_public_key() );
    generate_block(skip_flags);
    account_create( "bob", generate_private_key( "bob" ).get_public_key() );
    generate_block(skip_flags);

    db->pop_block();
    db->pop_block();
  } catch(const fc::exception& e) {
    edump( (e.to_detail_string()) );
    throw;
  }
}

BOOST_FIXTURE_TEST_CASE( rsf_missed_blocks, clean_database_fixture )
{
  try
  {
    generate_block();

    auto rsf = [&]() -> string
    {
      fc::uint128 rsf = db->get_dynamic_global_properties().recent_slots_filled;
      string result = "";
      result.reserve(128);
      for( int i=0; i<128; i++ )
      {
        result += ((rsf.lo & 1) == 0) ? '0' : '1';
        rsf >>= 1;
      }
      return result;
    };

    auto pct = []( uint32_t x ) -> uint32_t
    {
      return uint64_t( HIVE_100_PERCENT ) * x / 128;
    };

    BOOST_TEST_MESSAGE("checking initial participation rate" );
    BOOST_CHECK_EQUAL( rsf(),
      "1111111111111111111111111111111111111111111111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), static_cast<uint32_t>(HIVE_100_PERCENT) );

    BOOST_TEST_MESSAGE("Generating a block skipping 1" );
    generate_block( ~database::skip_fork_db, init_account_priv_key, 1 );
    BOOST_CHECK_EQUAL( rsf(),
      "0111111111111111111111111111111111111111111111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(127) );

    BOOST_TEST_MESSAGE("Generating a block skipping 1" );
    generate_block( ~database::skip_fork_db, init_account_priv_key, 1 );
    BOOST_CHECK_EQUAL( rsf(),
      "0101111111111111111111111111111111111111111111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(126) );

    BOOST_TEST_MESSAGE("Generating a block skipping 2" );
    generate_block( ~database::skip_fork_db, init_account_priv_key, 2 );
    BOOST_CHECK_EQUAL( rsf(),
      "0010101111111111111111111111111111111111111111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(124) );

    BOOST_TEST_MESSAGE("Generating a block for skipping 3" );
    generate_block( ~database::skip_fork_db, init_account_priv_key, 3 );
    BOOST_CHECK_EQUAL( rsf(),
      "0001001010111111111111111111111111111111111111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(121) );

    BOOST_TEST_MESSAGE("Generating a block skipping 5" );
    generate_block( ~database::skip_fork_db, init_account_priv_key, 5 );
    BOOST_CHECK_EQUAL( rsf(),
      "0000010001001010111111111111111111111111111111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(116) );

    BOOST_TEST_MESSAGE("Generating a block skipping 8" );
    generate_block( ~database::skip_fork_db, init_account_priv_key, 8 );
    BOOST_CHECK_EQUAL( rsf(),
      "0000000010000010001001010111111111111111111111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(108) );

    BOOST_TEST_MESSAGE("Generating a block skipping 13" );
    generate_block( ~database::skip_fork_db, init_account_priv_key, 13 );
    BOOST_CHECK_EQUAL( rsf(),
      "0000000000000100000000100000100010010101111111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(95) );

    BOOST_TEST_MESSAGE("Generating a block skipping none" );
    generate_block();
    BOOST_CHECK_EQUAL( rsf(),
      "1000000000000010000000010000010001001010111111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(95) );

    BOOST_TEST_MESSAGE("Generating a block" );
    generate_block();
    BOOST_CHECK_EQUAL( rsf(),
      "1100000000000001000000001000001000100101011111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(95) );

    generate_block();
    BOOST_CHECK_EQUAL( rsf(),
      "1110000000000000100000000100000100010010101111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(95) );

    generate_block();
    BOOST_CHECK_EQUAL( rsf(),
      "1111000000000000010000000010000010001001010111111111111111111111"
      "1111111111111111111111111111111111111111111111111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(95) );

    generate_block( ~database::skip_fork_db, init_account_priv_key, 64 );
    BOOST_CHECK_EQUAL( rsf(),
      "0000000000000000000000000000000000000000000000000000000000000000"
      "1111100000000000001000000001000001000100101011111111111111111111"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(31) );

    generate_block( ~database::skip_fork_db, init_account_priv_key, 32 );
    BOOST_CHECK_EQUAL( rsf(),
      "0000000000000000000000000000000010000000000000000000000000000000"
      "0000000000000000000000000000000001111100000000000001000000001000"
    );
    BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(8) );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE( skip_block, clean_database_fixture )
{
  try
  {
    BOOST_TEST_MESSAGE( "Skipping blocks through db" );
    BOOST_REQUIRE( db->head_block_num() == 2 );

    witness::block_producer bp( *db );
    unsigned int init_block_num = db->head_block_num();
    int miss_blocks = fc::minutes( 1 ).to_seconds() / HIVE_BLOCK_INTERVAL;
    auto witness = db->get_scheduled_witness( miss_blocks );
    auto block_time = db->get_slot_time( miss_blocks );
    GENERATE_BLOCK( bp, block_time , witness, init_account_priv_key, 0 );

    BOOST_CHECK_EQUAL( db->head_block_num(), init_block_num + 1 );
    BOOST_CHECK( db->head_block_time() == block_time );

    BOOST_TEST_MESSAGE( "Generating a block through fixture" );
    generate_block();

    BOOST_CHECK_EQUAL( db->head_block_num(), init_block_num + 2 );
    BOOST_CHECK( db->head_block_time() == block_time + HIVE_BLOCK_INTERVAL );
  }
  FC_LOG_AND_RETHROW();
}

BOOST_FIXTURE_TEST_CASE( hardfork_test, database_fixture )
{
  try
  {
    autoscope auto_wipe( [&]()
    {
      if( ah_plugin )
        ah_plugin->plugin_shutdown();
      if( data_dir )
        db->wipe( data_dir->path(), data_dir->path(), true );
    } );

    try {

    auto _data_dir = common_init( [&]( appbase::application& app, int argc, char** argv )
    {
      ah_plugin = &app.register_plugin< ah_plugin_type >();
      ah_plugin->set_destroy_database_on_startup();
      ah_plugin->set_destroy_database_on_shutdown();
      db_plugin = &app.register_plugin< hive::plugins::debug_node::debug_node_plugin >();

      app.initialize<
        ah_plugin_type,
        hive::plugins::debug_node::debug_node_plugin
      >( argc, argv );

      db = &app.get_plugin< hive::plugins::chain::chain_plugin >().db();
      BOOST_REQUIRE( db );
    } );
    
    init_account_pub_key = init_account_priv_key.get_public_key();

    ah_plugin->plugin_startup();

    open_database( _data_dir );

    generate_blocks( 2 );

    vest( "initminer", 10000 );

    // Fill up the rest of the required miners
    for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
    {
      account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
      fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD.amount.value );
      witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
    }

    validate_database();
    } catch ( const fc::exception& e )
    {
      edump( (e.to_detail_string()) );
      throw;
    }

    BOOST_TEST_MESSAGE( "Check hardfork not applied at genesis" );
    BOOST_REQUIRE( db->has_hardfork( 0 ) );
    BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_0_1 ) );

    BOOST_TEST_MESSAGE( "Generate blocks up to the hardfork time and check hardfork still not applied" );
    generate_blocks( fc::time_point_sec( HIVE_HARDFORK_0_1_TIME - HIVE_BLOCK_INTERVAL ), true );

    BOOST_REQUIRE( db->has_hardfork( 0 ) );
    BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_0_1 ) );

    const auto& ah_idx = db->get_index< hive::plugins::account_history_rocksdb::volatile_operation_index, by_id >();
    auto itr = ah_idx.end();
    itr--;

    const auto last_ah_id = itr->get_id();

    BOOST_TEST_MESSAGE( "Generate a block and check hardfork is applied" );
    generate_block();

    string op_msg = "Testnet: Hardfork applied";

    itr = ah_idx.upper_bound(last_ah_id);
    /// Skip another producer_reward_op generated at last produced block
    ++itr;

    ilog("Looked up AH-id: ${a}, found: ${i}", ("a", last_ah_id)("i", itr->get_id()));

    /// Original AH record points (by_id) actual operation data. We need to query for it again
    auto last_op = fc::raw::unpack_from_vector< hive::chain::operation >(itr->serialized_op);

    BOOST_REQUIRE( db->has_hardfork( 0 ) );
    BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_0_1 ) );
    operation hardfork_vop = hardfork_operation( HIVE_HARDFORK_0_1 );

    BOOST_REQUIRE(last_op == hardfork_vop);
    BOOST_REQUIRE(itr->timestamp == db->head_block_time());

    BOOST_TEST_MESSAGE( "Testing hardfork is only applied once" );
    generate_block();

    auto processed_op = last_op;

    /// Continue (starting from last HF op position), but skip last HF op
    for(++itr; itr != ah_idx.end(); ++itr)
    {
      processed_op = fc::raw::unpack_from_vector< hive::chain::operation >(itr->serialized_op);
    }

      /// There shall be no more hardfork ops after last one.
    BOOST_REQUIRE(processed_op.which() != hardfork_vop.which());

    BOOST_REQUIRE( db->has_hardfork( 0 ) );
    BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_0_1 ) );

    /// Search again for pre-HF operation
    itr = ah_idx.upper_bound(last_ah_id);
    /// Skip another producer_reward_op generated at last produced block
    ++itr;

    /// Here last HF vop shall be pointed, with its original time.
    BOOST_REQUIRE( itr->timestamp == db->head_block_time() - HIVE_BLOCK_INTERVAL );

  }
  catch( fc::exception& e )
  {
    throw e;
  }
  catch( std::exception& e )
  {
    throw e;
  }
}

BOOST_FIXTURE_TEST_CASE( generate_block_size, clean_database_fixture )
{
  try
  {
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT;
      });
    });
    generate_block();

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    transfer_operation op;
    op.from = HIVE_INIT_MINER_NAME;
    op.to = HIVE_TEMP_ACCOUNT;
    op.amount = asset( 1000, HIVE_SYMBOL );

    // tx minus op is 79 bytes
    // op is 33 bytes (32 for op + 1 byte static variant tag)
    // total is 65254
    // Original generation logic only allowed 115 bytes for the header
    // We are targetting a size (minus header) of 65421 which creates a block of "size" 65535
    // This block will actually be larger because the header estimates is too small

    for( size_t i = 0; i < 1975; i++ )
    {
      tx.operations.push_back( op );
    }

    sign( tx, init_account_priv_key );
    push_transaction( tx, 0 );

    // Second transaction, tx minus op is 78 (one less byte for operation vector size)
    // We need a 88 byte op. We need a 22 character memo (1 byte for length) 55 = 32 (old op) + 55 + 1
    op.memo = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ123";
    tx.clear();
    tx.operations.push_back( op );
    sign( tx, init_account_priv_key );
    push_transaction( tx, 0 );

    generate_block();

    // The last transfer should have been delayed due to size
    auto head_block = db->fetch_block_by_number( db->head_block_num() );
    BOOST_REQUIRE( head_block->get_block().transactions.size() == 1 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( set_lower_lib_then_current )
{
  try {
    // this is required to reproduce issue with setting last irreversible block
    // before block number HIVE_START_MINER_VOTING_BLOCK, if not then other test should be added
    BOOST_REQUIRE( HIVE_MAX_WITNESSES + 1 < HIVE_START_MINER_VOTING_BLOCK );

    fc::temp_directory data_dir( hive::utilities::temp_directory_path() );
    database db;
    witness::block_producer bp( db );
    db._log_hardforks = false;
    open_test_database( db, data_dir.path() );

    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")) );
    {
      uint32_t head_block_to_check = HIVE_MAX_WITNESSES - 3;
      for( uint32_t i = 0; i < head_block_to_check; ++i )
      {
        GENERATE_BLOCK( bp, db.get_slot_time(1), db.get_scheduled_witness(1),
          init_account_priv_key, database::skip_nothing );
      }

      db.set_last_irreversible_block_num(db.head_block_num());
      GENERATE_BLOCK( bp, db.get_slot_time(1), db.get_scheduled_witness(1),
        init_account_priv_key, database::skip_nothing );
    }

    {
      uint32_t head_block_to_check = (HIVE_MAX_WITNESSES + HIVE_START_MINER_VOTING_BLOCK) / 2;
      for( uint32_t i = db.head_block_num(); i < head_block_to_check; ++i )
      {
        GENERATE_BLOCK( bp, db.get_slot_time(1), db.get_scheduled_witness(1),
          init_account_priv_key, database::skip_nothing );
      }

      db.set_last_irreversible_block_num(db.head_block_num());
      GENERATE_BLOCK( bp, db.get_slot_time(1), db.get_scheduled_witness(1),
        init_account_priv_key, database::skip_nothing );
    }

    {
      uint32_t head_block_to_check = HIVE_START_MINER_VOTING_BLOCK + 3;
      for( uint32_t i = db.head_block_num(); i < head_block_to_check; ++i )
      {
        GENERATE_BLOCK( bp, db.get_slot_time(1), db.get_scheduled_witness(1),
          init_account_priv_key, database::skip_nothing );
      }

      db.set_last_irreversible_block_num(db.head_block_num());
      GENERATE_BLOCK( bp, db.get_slot_time(1), db.get_scheduled_witness(1),
        init_account_priv_key, database::skip_nothing );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( safe_closing_database )
{
  try {
    database db;
    fc::temp_directory data_dir( hive::utilities::temp_directory_path() );
    db.wipe( data_dir.path(), data_dir.path(), true );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif

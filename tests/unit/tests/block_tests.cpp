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

#include <hive/chain/block_storage_interface.hpp>
#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/sync_block_writer.hpp>

#include <hive/protocol/exceptions.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/external_storage/placeholder_comment_archive.hpp>
#include <hive/chain/external_storage/rocksdb_account_archive.hpp>

#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>
#include <hive/plugins/witness/block_producer.hpp>

#include <hive/utilities/tempdir.hpp>
#include <hive/utilities/database_configuration.hpp>

#include <fc/crypto/digest.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins;

#define TEST_SHARED_MEM_SIZE (1024 * 1024 * database_fixture::shared_file_size_small)

namespace fc
{
  std::ostream& boost_test_print_type(std::ostream& ostr, const ripemd160& hash)
  {
    ostr << hash.str();
    return ostr;
  }
  std::ostream& operator<<(std::ostream& ostr, const time_point_sec time)
  {
    ostr << time.to_iso_string();
    return ostr;
  }
}
namespace hive 
{
  namespace protocol
  {
    template <typename Storage>
    std::ostream& operator<<(std::ostream& ostr, const fixed_string_impl<Storage>& str)
    {
      ostr << (std::string)str;
      return ostr;
    }
  }
}

namespace std {
//see https://stackoverflow.com/questions/17572583/boost-check-fails-to-compile-operator-for-custom-types/17573165#17573165

std::ostream& operator<<( std::ostream& o, const block_flow_control::phase& p )
{
  switch( p )
  {
  case block_flow_control::phase::REQUEST: return o << "REQUEST";
  case block_flow_control::phase::START: return o << "START";
  case block_flow_control::phase::FORK_DB: return o << "FORK_DB";
  case block_flow_control::phase::FORK_APPLY: return o << "FORK_APPLY";
  case block_flow_control::phase::FORK_IGNORE: return o << "FORK_IGNORE";
  case block_flow_control::phase::FORK_NORMAL: return o << "FORK_NORMAL";
  case block_flow_control::phase::TXS_EXECUTED: return o << "TXS_EXECUTED";
  case block_flow_control::phase::APPLIED: return o << "APPLIED";
  case block_flow_control::phase::END: return o << "END";
  }
  return o << "<broken 'phase' enum value>";
}

}

BOOST_AUTO_TEST_SUITE(block_tests)

void open_test_database( database& db, block_storage_i& block_storage,
  const fc::path& dir, appbase::application& app, bool log_hardforks = false )
{
  auto comment_archive = std::make_shared<placeholder_comment_archive>( db );
  auto account_archive = std::make_shared<rocksdb_account_archive>( db, dir, dir / "accounts-rocksdb-storage", app );

  db.set_comments_handler( comment_archive );
  db.set_accounts_handler( account_archive );

  hive::chain::open_args args;
  hive::chain::block_storage_i::block_log_open_args bl_args;
  args.data_dir = dir;
  args.shared_mem_dir = dir;
  args.shared_file_size = TEST_SHARED_MEM_SIZE;
  args.database_cfg = hive::utilities::default_database_configuration();
  configuration_data.set_initial_asset_supply( INITIAL_TEST_SUPPLY, HBD_INITIAL_TEST_SUPPLY );
  db._log_hardforks = log_hardforks;
  bl_args.data_dir = dir;
  db.pre_open( args );
  db.with_write_lock([&]()
  {
    block_storage.open_and_init( bl_args, true/*read_only*/, &db );
  });
  db.open( args );
}

#define SET_UP_FIXTURE( REMOVE_DB_FILES, DATA_DIR_PATH_STR ) \
  hived_fixture fixture( REMOVE_DB_FILES ); \
  fixture.postponed_init( \
    { \
      hived_fixture::config_line_t( { "shared-file-dir", \
        { DATA_DIR_PATH_STR } } \
      ) \
    } \
  ); \
  database& db = *(fixture.db); \
  hive::plugins::chain::chain_plugin& chain_plugin = fixture.get_chain_plugin();

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
      SET_UP_FIXTURE( true /*REMOVE_DB_FILES*/, data_dir.path().string() );
      const block_read_i& block_reader = chain_plugin.block_reader();
      witness::block_producer bp( chain_plugin );
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
          auto block = block_reader.get_block_by_number( cutoff_height );
          BOOST_REQUIRE( block );
          cutoff_block = block;
          break;
        }
      }
    }
    {
      SET_UP_FIXTURE( false /*REMOVE_DB_FILES*/, data_dir.path().string() );
      witness::block_producer bp( chain_plugin );

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
      SET_UP_FIXTURE( true /*REMOVE_DB_FILES*/, data_dir.path().string() );
      witness::block_producer bp( chain_plugin );
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

#define SET_UP_FIXTURE_SUFFIX( DATA_DIR_PATH_STR, SUFFIX, COMMON_LOGGING_CONFIG ) \
  hived_fixture fixture ## SUFFIX( true /*remove_db_files*/, true/*disable_p2p*/, DATA_DIR_PATH_STR ); \
  fixture ## SUFFIX.set_logging_config( COMMON_LOGGING_CONFIG ); \
  fixture ## SUFFIX.postponed_init( \
    { \
      hived_fixture::config_line_t( { "shared-file-dir", \
        { DATA_DIR_PATH_STR } } \
      ) \
    } \
  ); \
  database& db ## SUFFIX = *(fixture ## SUFFIX.db); \
  hive::plugins::chain::chain_plugin& chain_plugin ## SUFFIX = fixture ## SUFFIX.get_chain_plugin();

BOOST_AUTO_TEST_CASE( fork_blocks )
{
  try {
    fc::temp_directory data_dir1( hive::utilities::temp_directory_path() );
    SET_UP_FIXTURE_SUFFIX( data_dir1.path().string(), 1, fc::optional< fc::logging_config >() );
    auto common_logging_config = fixture1.get_logging_config();
    fc::temp_directory data_dir2( hive::utilities::temp_directory_path() );
    SET_UP_FIXTURE_SUFFIX( data_dir2.path().string(), 2, common_logging_config );

    //TODO This test needs 6-7 ish witnesses prior to fork

    witness::block_producer bp1( chain_plugin1 );
    witness::block_producer bp2( chain_plugin2 );

    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")) );
    for( uint32_t i = 0; i < 10; ++i )
    {
      auto b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
                               init_account_priv_key, database::skip_nothing );
      try {
        PUSH_BLOCK( chain_plugin2, b );
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
      PUSH_BLOCK( chain_plugin1, b );
      BOOST_CHECK_EQUAL(db1.head_block_id().str(), db1_tip);
      BOOST_CHECK_EQUAL(db2.head_block_id().str(), b->get_block_id().str());
    }

    string db2_block_13_id = db2.head_block_id().str();
    //The two databases are on distinct forks now, but at the same height. Make a block on db2, make it invalid, then
    //pass it to db1 and assert that db1 successfully switches to the new fork, but only as far as block 13, the last
    //valid block
    std::shared_ptr<full_block_type> good_block;
    BOOST_CHECK_EQUAL(db1.head_block_num(), 13u);
    BOOST_CHECK_EQUAL(db2.head_block_num(), 13u);
    {
      auto b = GENERATE_BLOCK( bp2, db2.get_slot_time(1), db2.get_scheduled_witness(1),
                               init_account_priv_key, database::skip_nothing );
      good_block = b;
      auto bad_block_header = b->get_block_header();
      auto bad_block_txs = b->get_full_transactions();
      signed_transaction tx;
      tx.operations.emplace_back( transfer_operation() );
      bad_block_txs.emplace_back( full_transaction_type::create_from_signed_transaction( tx, pack_type::legacy, false ) );
      BOOST_CHECK_EQUAL(bad_block_header.block_num(), 14u);
      HIVE_CHECK_THROW(PUSH_BLOCK( chain_plugin1, bad_block_header, bad_block_txs, init_account_priv_key ), fc::exception);
    }
    BOOST_CHECK_EQUAL(db1.head_block_num(), 13u);
    BOOST_CHECK_EQUAL(db1.head_block_id().str(), db2_block_13_id);

    // assert that db1 accepts the good version of block 14
    BOOST_CHECK_EQUAL(db2.head_block_num(), 14u);
    PUSH_BLOCK( chain_plugin1, good_block );
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
    SET_UP_FIXTURE_SUFFIX( dir1.path().string(), 1, fc::optional< fc::logging_config >() );
    auto common_logging_config = fixture1.get_logging_config();
    SET_UP_FIXTURE_SUFFIX( dir2.path().string(), 2, common_logging_config );
    witness::block_producer bp1( chain_plugin1 ),
                    bp2( chain_plugin2 );

    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")) );
    public_key_type init_account_pub_key  = init_account_priv_key.get_public_key();
    db1.get_index< account_index >();

    {
      //Generate an empty block, because `runtime_expiration` for every transaction is set only when `head_block_num() > 0`
      auto b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
        init_account_priv_key, database::skip_nothing );
      PUSH_BLOCK( chain_plugin1, b );
    }
    {
      //Generate an empty block, because `runtime_expiration` for every transaction is set only when `head_block_num() > 0`
      auto b = GENERATE_BLOCK( bp2, db2.get_slot_time(1), db2.get_scheduled_witness(1),
        init_account_priv_key, database::skip_nothing );
      PUSH_BLOCK( chain_plugin2, b );
    }

    //*
    signed_transaction trx;
    account_create_operation cop;
    cop.new_account_name = "alice";
    cop.creator = HIVE_INIT_MINER_NAME;
    cop.owner = authority(1, init_account_pub_key, 1);
    cop.active = cop.owner;
    trx.operations.push_back(cop);
    trx.set_expiration( db1.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    PUSH_TX( chain_plugin1, trx, init_account_priv_key );
    //*/
    // generate blocks
    // db1 : A
    // db2 : B C D

    auto b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );

    auto alice_id = db1.get_account( "alice" ).get_account_id();
    BOOST_CHECK( db1.get(alice_id).get_name() == "alice" );

    b = GENERATE_BLOCK( bp2, db2.get_slot_time(1), db2.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    PUSH_BLOCK( chain_plugin1, b );
    b = GENERATE_BLOCK( bp2, db2.get_slot_time(1), db2.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    PUSH_BLOCK( chain_plugin1, b );
    HIVE_REQUIRE_THROW(db2.get(alice_id), std::exception);
    db1.get(alice_id); /// it should be included in the pending state
    db1.clear_pending(); // clear it so that we can verify it was properly removed from pending state.
    HIVE_REQUIRE_THROW(db1.get(alice_id), std::exception);

    PUSH_TX( chain_plugin2, trx, init_account_priv_key );

    b = GENERATE_BLOCK( bp2, db2.get_slot_time(1), db2.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    PUSH_BLOCK( chain_plugin1, b );

    BOOST_CHECK( db1.get(alice_id).get_name() == "alice");
    BOOST_CHECK( db2.get(alice_id).get_name() == "alice");
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

namespace
{ 
  __attribute__((unused))
  void dump_witnesses(std::string_view label, const hive::chain::database& db)
  {
    const witness_schedule_object& wso_for_irreversibility = db.get_witness_schedule_object_for_irreversibility();
    BOOST_TEST_MESSAGE(label << ": head block #" << db.head_block_num() << ", " << (int)wso_for_irreversibility.num_scheduled_witnesses << " scheduled witnesses");

    const auto fast_confirming_witnesses = boost::make_iterator_range(wso_for_irreversibility.current_shuffled_witnesses.begin(),
                                                                      wso_for_irreversibility.current_shuffled_witnesses.begin() + 
                                                                      wso_for_irreversibility.num_scheduled_witnesses);
    std::ostringstream wits;
    bool first = true;
    for (const account_name_type& fast_confirming_witness : fast_confirming_witnesses)
    {
      wits << (first ? "" : ", ") << fast_confirming_witness;
      first = false;
    }
    BOOST_TEST_MESSAGE(label << ": witnesses: " << wits.str());
  }

  __attribute__((unused))
  void dump_blocks(std::string_view label, const hive::chain::database& db,
                   const block_read_i& block_reader)
  {
    BOOST_TEST_MESSAGE(label << ": last_irrevresible_block: " << db.get_last_irreversible_block_num());
    for (uint32_t block_num = 1; block_num <= db.head_block_num(); ++block_num)
    {
      std::shared_ptr<full_block_type> block = block_reader.get_block_by_number(block_num);
      BOOST_TEST_MESSAGE(label << ": block #" << block->get_block_num() << 
                         ", id " << block->get_block_id().str() << 
                         " generated by " << block->get_block_header().witness);
    }
  }
}

#define SET_UP_CLEAN_DATABASE_FIXTURE_SUFFIX( SUFFIX ) \
  clean_database_fixture fixture ## SUFFIX( \
    database_fixture::shared_file_size_big, \
    fc::optional<uint32_t>(), \
    false /*init_ah_plugin*/ ); \
  database& db ## SUFFIX = *(fixture ## SUFFIX.db);

// Normally, the only thing that will cause the blockchain to switch to a different for
// is if the new fork is longer than the current fork.  But with the fast-confirmation
// transactions, if we find that a supermajority of witnesses support a different fork
// from our own, and the fork is the same length as our current fork, we should switch.
//
// This test creates a fork between the database controlled by the fixture and a second
// database, manually passing blocks and transactions between the two similar to how
// a flaky network might.
// We'll assume this is a case where a single witness (on db2) gets disconnected from 
// the network, while the other 20 witnesses remain connected (on db1)
BOOST_AUTO_TEST_CASE(switch_forks_using_fast_confirm)
{
  try
  {
    SET_UP_CLEAN_DATABASE_FIXTURE_SUFFIX( 1 )
    auto common_logging_config = fixture1.get_logging_config();
    hive::plugins::chain::chain_plugin& chain_plugin1 = fixture1.get_chain_plugin();

    fc::ecc::private_key init_account_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")));

    BOOST_TEST_MESSAGE("Generating several rounds of blocks to allow our real witnesses to become active");
    BOOST_TEST_MESSAGE("db1 head_block_num = " << db1.head_block_num());
    fixture1.generate_blocks(63);
    // dump_witnesses("db1", *db);
     dump_blocks("db1", db1, fixture1.get_block_reader());
    BOOST_REQUIRE_EQUAL(db1.head_block_num(), 65);

    fc::temp_directory dir2(hive::utilities::temp_directory_path());
    // create a second, empty, database that we will first bring in sync with the 
    // fixture's database, then we will trigger a fork and test how it resolves.
    // we'll call the fixture's database "db1"
    SET_UP_FIXTURE_SUFFIX( dir2.path().string(), 2, common_logging_config );

    BOOST_TEST_MESSAGE("db2 head_block_num = " << db2.head_block_num());
    // dump_witnesses("db2", db2);
    // dump_blocks("db2", db2);

    BOOST_TEST_MESSAGE("Copying initial blocks generated in db1 to db2");
    for (uint32_t block_num = 1; block_num <= db1.head_block_num(); ++block_num)
    {
      std::shared_ptr<full_block_type> block = fixture1.get_block_reader().get_block_by_number(block_num);
      PUSH_BLOCK(chain_plugin2, block);
      // the fixture applies the hardforks to db1 after block 1, do the same thing to db2 here
      if (block_num == 1)
        db2.set_hardfork(26);
    }

    BOOST_TEST_MESSAGE("db2 head_block_num = " << db2.head_block_num());
    BOOST_REQUIRE_EQUAL(db1.head_block_id(), db2.head_block_id());
    BOOST_TEST_MESSAGE("db: head block is " << db1.head_block_id().str());
    BOOST_TEST_MESSAGE("db1 head_block_num = " << db1.head_block_num());
    // dump_witnesses("db2", db2);
    // dump_blocks("db2", db2);
    BOOST_TEST_MESSAGE("db2 last_irreversible_block is " << db2.get_last_irreversible_block_num());

    // db1 and db2 are now in sync, but their last_irreversible should be about 15 blocks old.  Go ahead and
    // use fast-confirms to bring the last_irreversible up to the current head block.
    // (this isn't strictly necessary to achieve the goal of this test case, but it's probably more 
    // representative of how the blockchains will look in real life)
    {
      BOOST_TEST_MESSAGE("Broadcasting fast-confirm transactions for all blocks " << db2.get_last_irreversible_block_num());
      const witness_schedule_object& wso_for_irreversibility = db1.get_witness_schedule_object_for_irreversibility();
      const auto fast_confirming_witnesses = boost::make_iterator_range(wso_for_irreversibility.current_shuffled_witnesses.begin(),
                                                                        wso_for_irreversibility.current_shuffled_witnesses.begin() + 
                                                                        wso_for_irreversibility.num_scheduled_witnesses);
      std::shared_ptr<full_block_type> full_head_block = fixture1.get_block_reader().get_block_by_number(db1.head_block_num());
      const account_name_type witness_for_head_block = full_head_block->get_block_header().witness;
      for (const account_name_type& fast_confirming_witness : fast_confirming_witnesses)
        if (fast_confirming_witness != witness_for_head_block) // the wit that generated the block doesn't fast-confirm their own block
        {
          BOOST_TEST_MESSAGE("Confirming head block with witness " << fast_confirming_witness);
          witness_block_approve_operation fast_confirm_op;
          fast_confirm_op.witness = fast_confirming_witness;
          fast_confirm_op.block_id = full_head_block->get_block_id();
          fixture1.push_transaction(fast_confirm_op, init_account_priv_key);

          signed_transaction trx;
          trx.operations.push_back(fast_confirm_op);
          trx.set_expiration(db2.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
          PUSH_TX(chain_plugin2, trx, init_account_priv_key);
        }
    }
    BOOST_REQUIRE_EQUAL(db1.get_last_irreversible_block_num(), db1.head_block_num());
    BOOST_REQUIRE_EQUAL(db2.get_last_irreversible_block_num(), db2.head_block_num());

    // fork.  db2 will generate the next block, but we won't propagate it.
    // then db1 will generate the a block at the same height.
    BOOST_TEST_MESSAGE("Simulating a fork by generating a block in db2 that won't be shared with db1");
    witness::block_producer block_producer2( chain_plugin2 );
    const fc::time_point_sec head_block_time = db2.head_block_time();
    const fc::time_point_sec orphan_slot_time = head_block_time + HIVE_BLOCK_INTERVAL;
    const fc::time_point_sec real_slot_time = orphan_slot_time + HIVE_BLOCK_INTERVAL;
    BOOST_TEST_MESSAGE("head block time is " << head_block_time << ", orphan block time will be " << orphan_slot_time);
    const uint32_t orphan_slot_num = db2.get_slot_at_time(orphan_slot_time);
    const uint32_t real_slot_num = db2.get_slot_at_time(real_slot_time);
    BOOST_TEST_MESSAGE("head slot num is " << db2.get_slot_at_time(head_block_time) << ", orphan slot num will be " << orphan_slot_num);
    BOOST_TEST_MESSAGE("On db1 blockchain, we'll skip the block that should have been produced at time " << orphan_slot_time << 
                       " by " << db2.get_scheduled_witness(orphan_slot_num) << ", and instead produce the block at time " <<
                       real_slot_time << " with " << db2.get_scheduled_witness(real_slot_num));
    std::shared_ptr<full_block_type> orphan_block = GENERATE_BLOCK(block_producer2, 
                                                                   orphan_slot_time, 
                                                                   db2.get_scheduled_witness(orphan_slot_num), 
                                                                   init_account_priv_key, database::skip_nothing);
    BOOST_TEST_MESSAGE("Generated block #" << orphan_block->get_block_num() << " with id " << orphan_block->get_block_id().str() <<
                       " generated by witness " << orphan_block->get_block_header().witness << ", pushing to db2");
    PUSH_BLOCK(chain_plugin2, orphan_block);
    BOOST_REQUIRE_EQUAL(orphan_block->get_block_num(), db2.head_block_num());
    BOOST_REQUIRE_EQUAL(orphan_block->get_block_id(), db2.head_block_id());
    BOOST_REQUIRE_EQUAL(db2.get_last_irreversible_block_num(), db2.head_block_num() - 1);
    BOOST_REQUIRE_EQUAL(orphan_block->get_block_header().timestamp, orphan_slot_time);

    BOOST_TEST_MESSAGE("Creating the other side of the fork by generating a block in db1 that doesn't include db2's head block");
    db2.get_scheduled_witness(db2.head_block_num() + 1), 
    fixture1.generate_block(0, init_account_priv_key, 1 /* <-- skip one block */);
    std::shared_ptr<full_block_type> real_block = fixture1.get_block_reader().get_block_by_number(db1.head_block_num());
    BOOST_TEST_MESSAGE("Generated block #" << real_block->get_block_num() << " with id " << real_block->get_block_id().str() <<
                       " generated by witness " << real_block->get_block_header().witness << ", pushed to db1");
    BOOST_REQUIRE_EQUAL(real_block->get_block_num(), orphan_block->get_block_num());
    BOOST_REQUIRE_EQUAL(real_block->get_block_header().timestamp, real_slot_time);
    BOOST_REQUIRE_NE(orphan_block->get_block_id(), real_block->get_block_id());
    BOOST_REQUIRE_EQUAL(db1.head_block_num(), db2.head_block_num());
    BOOST_REQUIRE_NE(db1.head_block_id(), db2.head_block_id());

    // reconnect
    BOOST_TEST_MESSAGE("Reconnecting the two networks");
    PUSH_BLOCK(chain_plugin1, orphan_block);
    PUSH_BLOCK(chain_plugin2, real_block);
    // nothing should happen yet
    BOOST_REQUIRE_EQUAL(orphan_block->get_block_id(), db2.head_block_id());
    BOOST_REQUIRE_EQUAL(real_block->get_block_id(), db1.head_block_id());

    // now let db2 see the fast-confirm messages of all the 20 other witnesses that were on db1
    {
      BOOST_TEST_MESSAGE("Broadcasting fast-confirm for the new non-orphan head block");
      const witness_schedule_object& wso_for_irreversibility = db1.get_witness_schedule_object_for_irreversibility();
      const auto fast_confirming_witnesses = boost::make_iterator_range(wso_for_irreversibility.current_shuffled_witnesses.begin(),
                                                                        wso_for_irreversibility.current_shuffled_witnesses.begin() + 
                                                                        wso_for_irreversibility.num_scheduled_witnesses);
      const account_name_type witness_for_real_block = real_block->get_block_header().witness;
      const account_name_type witness_for_orphan_block = orphan_block->get_block_header().witness;
      for (const account_name_type& fast_confirming_witness : fast_confirming_witnesses)
        if (fast_confirming_witness != witness_for_real_block && // the wit that generated the block doesn't fast-confirm their own block
            fast_confirming_witness != witness_for_orphan_block) // and the wit from db2 won't, because his head is still the orphan block
        {
          BOOST_TEST_MESSAGE("Confirming real block with witness " << fast_confirming_witness);
          witness_block_approve_operation fast_confirm_op;
          fast_confirm_op.witness = fast_confirming_witness;
          fast_confirm_op.block_id = real_block->get_block_id();
          fixture1.push_transaction(fast_confirm_op, init_account_priv_key);

          signed_transaction trx;
          trx.operations.push_back(fast_confirm_op);
          trx.set_expiration(db2.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
          PUSH_TX(chain_plugin2, trx, init_account_priv_key);
        }
    }

    BOOST_TEST_MESSAGE("Verifying that the forked node rejoins after receiving fast confirmations, even though the new chain isn't longer");
    BOOST_REQUIRE_EQUAL(db1.get_last_irreversible_block_num(), db1.head_block_num());
    BOOST_REQUIRE_EQUAL(db2.get_last_irreversible_block_num(), db2.head_block_num());
    BOOST_REQUIRE_EQUAL(db1.head_block_id(), db2.head_block_id());
    BOOST_REQUIRE_EQUAL(real_block->get_block_id(), db1.head_block_id());
  }
  catch (const fc::exception& e)
  {
    edump((e));
    throw;
  }
}

BOOST_AUTO_TEST_CASE(fast_confirm_plus_out_of_order_blocks)
{
  try
  {
    SET_UP_CLEAN_DATABASE_FIXTURE_SUFFIX( 1 )
    auto common_logging_config = fixture1.get_logging_config();

    fc::ecc::private_key init_account_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")));

    BOOST_TEST_MESSAGE("Generating several rounds of blocks to allow our real witnesses to become active");
    BOOST_TEST_MESSAGE("db1 head_block_num = " << db1.head_block_num());
    fixture1.generate_blocks(63);
    // dump_witnesses("db1", *db);
    // dump_blocks("db1", *db);
    BOOST_REQUIRE_EQUAL(db1.head_block_num(), 65);

    fc::temp_directory dir2(hive::utilities::temp_directory_path());
    // create a second, empty, database that we will first bring in sync with the 
    // fixture's database, then we will trigger a fork and test how it resolves.
    // we'll call the fixture's database "db1"
    SET_UP_FIXTURE_SUFFIX( dir2.path().string(), 2, common_logging_config );


    BOOST_TEST_MESSAGE("db2 head_block_num = " << db2.head_block_num());
    // dump_witnesses("db2", db2);
    // dump_blocks("db2", db2);

    BOOST_TEST_MESSAGE("Copying initial blocks generated in db1 to db2");
    for (uint32_t block_num = 1; block_num <= db1.head_block_num(); ++block_num)
    {
      std::shared_ptr<full_block_type> block = fixture1.get_block_reader().get_block_by_number(block_num);
      PUSH_BLOCK(chain_plugin2, block);
      // the fixture applies the hardforks to db1 after block 1, do the same thing to db2 here
      if (block_num == 1)
        db2.set_hardfork(26);
    }

    BOOST_TEST_MESSAGE("db2 head_block_num = " << db2.head_block_num());
    BOOST_REQUIRE_EQUAL(db1.head_block_id(), db2.head_block_id());
    BOOST_TEST_MESSAGE("db: head block is " << db1.head_block_id().str());
    BOOST_TEST_MESSAGE("db1 head_block_num = " << db1.head_block_num());
    // dump_witnesses("db2", db2);
    // dump_blocks("db2", db2);
    BOOST_TEST_MESSAGE("db2 last_irreversible_block is " << db2.get_last_irreversible_block_num());

    // db1 and db2 are now in sync, but their last_irreversible should be about 15 blocks old.  Go ahead and
    // use fast-confirms to bring the last_irreversible up to the current head block.
    // (this isn't strictly necessary to achieve the goal of this test case, but it's probably more 
    // representative of how the blockchains will look in real life)
    {
      BOOST_TEST_MESSAGE("Broadcasting fast-confirm transactions for all blocks " << db2.get_last_irreversible_block_num());
      const witness_schedule_object& wso_for_irreversibility = db1.get_witness_schedule_object_for_irreversibility();
      const auto fast_confirming_witnesses = boost::make_iterator_range(wso_for_irreversibility.current_shuffled_witnesses.begin(),
                                                                        wso_for_irreversibility.current_shuffled_witnesses.begin() + 
                                                                        wso_for_irreversibility.num_scheduled_witnesses);
      std::shared_ptr<full_block_type> full_head_block = fixture1.get_block_reader().get_block_by_number(db1.head_block_num());
      const account_name_type witness_for_head_block = full_head_block->get_block_header().witness;
      for (const account_name_type& fast_confirming_witness : fast_confirming_witnesses)
        if (fast_confirming_witness != witness_for_head_block) // the wit that generated the block doesn't fast-confirm their own block
        {
          BOOST_TEST_MESSAGE("Confirming head block with witness " << fast_confirming_witness);
          witness_block_approve_operation fast_confirm_op;
          fast_confirm_op.witness = fast_confirming_witness;
          fast_confirm_op.block_id = full_head_block->get_block_id();
          fixture1.push_transaction(fast_confirm_op, init_account_priv_key);

          signed_transaction trx;
          trx.operations.push_back(fast_confirm_op);
          trx.set_expiration(db2.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
          PUSH_TX(chain_plugin2, trx, init_account_priv_key);
        }
    }
    BOOST_REQUIRE_EQUAL(db1.get_last_irreversible_block_num(), db1.head_block_num());
    BOOST_REQUIRE_EQUAL(db2.get_last_irreversible_block_num(), db2.head_block_num());

    // now we want to create the next two blocks, but simulate the case where the blocks arrive
    // in the wrong order: i.e., we:
    // - receive all the fast-confirms for block 66
    // - we receive block 67
    // - we receive block 66

    // generate block 66 in db1, but don't propagate it to db2
    fixture1.generate_blocks(1);
    {
      BOOST_TEST_MESSAGE("Broadcasting fast-confirm transactions for block 66 for all witnesses " << db2.get_last_irreversible_block_num());
      const witness_schedule_object& wso_for_irreversibility = db1.get_witness_schedule_object_for_irreversibility();
      const auto fast_confirming_witnesses = boost::make_iterator_range(wso_for_irreversibility.current_shuffled_witnesses.begin(),
                                                                        wso_for_irreversibility.current_shuffled_witnesses.begin() + 
                                                                        wso_for_irreversibility.num_scheduled_witnesses);
      std::shared_ptr<full_block_type> full_head_block = fixture1.get_block_reader().get_block_by_number(db1.head_block_num());
      const account_name_type witness_for_head_block = full_head_block->get_block_header().witness;
      for (const account_name_type& fast_confirming_witness : fast_confirming_witnesses)
        if (fast_confirming_witness != witness_for_head_block) // the wit that generated the block doesn't fast-confirm their own block
        {
          BOOST_TEST_MESSAGE("Confirming block 66 with witness " << fast_confirming_witness);
          witness_block_approve_operation fast_confirm_op;
          fast_confirm_op.witness = fast_confirming_witness;
          fast_confirm_op.block_id = full_head_block->get_block_id();
          fixture1.push_transaction(fast_confirm_op, init_account_priv_key);

          // push the fast-confirms to db2, which doesn't yet have block 66
          signed_transaction trx;
          trx.operations.push_back(fast_confirm_op);
          trx.set_expiration(db2.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
          PUSH_TX(chain_plugin2, trx, init_account_priv_key);
        }
    }
    BOOST_REQUIRE_EQUAL(db1.head_block_num(), 66);
    BOOST_REQUIRE_EQUAL(db2.head_block_num(), 65);
    BOOST_REQUIRE_EQUAL(db1.get_last_irreversible_block_num(), 66);
    BOOST_REQUIRE_EQUAL(db2.get_last_irreversible_block_num(), 65);

    // generate block 67 in db1, propagate it to db2.  it will be pushed to the forkdb's unlinked index
    fixture1.generate_blocks(1);
    BOOST_REQUIRE_THROW(PUSH_BLOCK(chain_plugin2, fixture1.get_block_reader().get_block_by_number(67)), hive::chain::unlinkable_block_exception);

    // nothing should have changed
    BOOST_REQUIRE_EQUAL(db2.head_block_num(), 65);
    BOOST_REQUIRE_EQUAL(db2.get_last_irreversible_block_num(), 65);

    PUSH_BLOCK(chain_plugin2, fixture1.get_block_reader().get_block_by_number(66));

    // pushing block 66 has linked in block 67, so db2 should apply both blocks.  It already has the
    // fast-confirms for block 66, so it should become irreverisble immediately.
    BOOST_REQUIRE_EQUAL(db2.head_block_num(), 67);
    BOOST_REQUIRE_EQUAL(db2.get_last_irreversible_block_num(), 66);
  }
  catch (const fc::exception& e)
  {
    edump((e));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( duplicate_transactions )
{
  try {
    fc::temp_directory dir1( hive::utilities::temp_directory_path() ),
                  dir2( hive::utilities::temp_directory_path() );
    appbase::application app;  
    SET_UP_FIXTURE_SUFFIX( dir1.path().string(), 1, fc::optional< fc::logging_config >() );
    auto common_logging_config = fixture1.get_logging_config();
    SET_UP_FIXTURE_SUFFIX( dir2.path().string(), 2, common_logging_config );
    witness::block_producer bp1( chain_plugin1 );
    BOOST_CHECK( db1.get_chain_id() == db2.get_chain_id() );

    auto skip_sigs = database::skip_transaction_signatures | database::skip_authority_check;

    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")) );
    public_key_type init_account_pub_key  = init_account_priv_key.get_public_key();

    {
      //Generate an empty block, because `runtime_expiration` for every transaction is set only when `head_block_num() > 0`
      auto b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
        init_account_priv_key, skip_sigs );
      PUSH_BLOCK( chain_plugin2, b, skip_sigs );
    }

    signed_transaction trx;
    account_create_operation cop;
    cop.new_account_name = "alice";
    cop.creator = HIVE_INIT_MINER_NAME;
    cop.owner = authority(1, init_account_pub_key, 1);
    cop.active = cop.owner;
    trx.operations.push_back(cop);
    trx.set_expiration( db1.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    PUSH_TX( chain_plugin1, trx, init_account_priv_key, skip_sigs );

    trx = decltype(trx)();
    transfer_operation t;
    t.from = HIVE_INIT_MINER_NAME;
    t.to = "alice";
    t.amount = asset(500,HIVE_SYMBOL);
    trx.operations.push_back(t);
    trx.set_expiration( db1.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    PUSH_TX( chain_plugin1, trx, init_account_priv_key, skip_sigs );

    HIVE_CHECK_THROW(PUSH_TX( chain_plugin1, trx, init_account_priv_key, skip_sigs ), fc::exception);

    auto b = GENERATE_BLOCK( bp1, db1.get_slot_time(1), db1.get_scheduled_witness(1),
      init_account_priv_key, skip_sigs );
    PUSH_BLOCK( chain_plugin2, b, skip_sigs );

    HIVE_CHECK_THROW(PUSH_TX( chain_plugin1, trx, init_account_priv_key, skip_sigs ), fc::exception);
    HIVE_CHECK_THROW(PUSH_TX( chain_plugin2, trx, init_account_priv_key, skip_sigs ), fc::exception);
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
    fc::temp_directory data_dir( hive::utilities::temp_directory_path() );
    SET_UP_FIXTURE( true /*REMOVE_DB_FILES*/, data_dir.path().string() );
    witness::block_producer bp1( chain_plugin );

    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("init_key")) );
    public_key_type init_account_pub_key  = init_account_priv_key.get_public_key();

    auto b = GENERATE_BLOCK( bp1, db.get_slot_time(1), db.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );

    BOOST_TEST_MESSAGE( "Creating a transaction with reference block" );
    idump((db.head_block_id()));
    signed_transaction trx;
    //This transaction must be in the next block after its reference, or it is invalid.
    trx.set_reference_block( db.head_block_id() );

    account_create_operation cop;
    cop.new_account_name = "alice";
    cop.creator = HIVE_INIT_MINER_NAME;
    cop.owner = authority(1, init_account_pub_key, 1);
    cop.active = cop.owner;
    trx.operations.push_back(cop);
    trx.set_expiration( db.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    BOOST_TEST_MESSAGE( "Pushing Pending Transaction" );
    idump((trx));
    PUSH_TX( chain_plugin, trx, init_account_priv_key );
    BOOST_TEST_MESSAGE( "Generating a block" );
    b = GENERATE_BLOCK( bp1, db.get_slot_time(1), db.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    trx.clear();

    transfer_operation t;
    t.from = HIVE_INIT_MINER_NAME;
    t.to = "alice";
    t.amount = asset(50,HIVE_SYMBOL);
    trx.operations.push_back(t);
    trx.set_expiration( db.head_block_time() + fc::seconds(2) );
    idump((trx)(db.head_block_time()));
    b = GENERATE_BLOCK( bp1, db.get_slot_time(1), db.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    idump((b->get_block()));
    b = GENERATE_BLOCK( bp1, db.get_slot_time(1), db.get_scheduled_witness(1),
      init_account_priv_key, database::skip_nothing );
    BOOST_REQUIRE_THROW( PUSH_TX(chain_plugin, trx, init_account_priv_key, 0/*database::skip_transaction_signatures | database::skip_authority_check*/), fc::exception );
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

    fund( "alice", ASSET( "1000.000 TESTS" ) );
    transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.amount = asset(1000,HIVE_SYMBOL);
    signed_transaction tx;
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "ref_block_num=0, ref_block_prefix=0" );

    tx.ref_block_num = 0;
    tx.ref_block_prefix = 0;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key );

    BOOST_TEST_MESSAGE( "proper ref_block_num, ref_block_prefix" );

    tx.set_reference_block( db->head_block_id() );
    push_transaction( tx, alice_private_key );

    BOOST_TEST_MESSAGE( "ref_block_num=0, ref_block_prefix=12345678" );

    tx.ref_block_num = 0;
    tx.ref_block_prefix = 0x12345678;
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key ), transaction_tapos_exception );

    BOOST_TEST_MESSAGE( "ref_block_num=1, ref_block_prefix=12345678" );

    tx.ref_block_num = 1;
    tx.ref_block_prefix = 0x12345678;
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key ), transaction_tapos_exception );

    BOOST_TEST_MESSAGE( "ref_block_num=9999, ref_block_prefix=12345678" );

    tx.ref_block_num = 9999;
    tx.ref_block_prefix = 0x12345678;
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key ), transaction_tapos_exception );
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

  signed_transaction trx;
  transfer_operation t;
  t.from = HIVE_INIT_MINER_NAME;
  t.to = "bob";
  t.amount = asset(amount,HIVE_SYMBOL);
  trx.operations.push_back(t);
  trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  trx.validate();

  push_transaction( trx, init_account_priv_key );

  trx.operations.clear();
  t.from = "bob";
  t.to = HIVE_INIT_MINER_NAME;
  t.amount = asset(amount/2,HIVE_SYMBOL);
  trx.operations.push_back(t);
  trx.validate();

  BOOST_TEST_MESSAGE( "Verify that not-signing causes an exception" );
  HIVE_REQUIRE_THROW( push_transaction(trx), tx_missing_active_auth );

  BOOST_TEST_MESSAGE( "Verify that double-signing causes an exception" );
  HIVE_REQUIRE_THROW( push_transaction(trx, {bob_private_key, bob_private_key} ), tx_duplicate_sig );

  BOOST_TEST_MESSAGE( "Up to HF28 it was a verify that signing with an extra, unused key fails. Now the transaction passes." );
  push_transaction( trx, { bob_private_key, generate_private_key( "bogus" ) } );

  BOOST_TEST_MESSAGE( "Verify that signing once with the proper key passes" );
  trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION - HIVE_BLOCK_INTERVAL );
  push_transaction( trx, bob_private_key );

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
    fund( "sam", ASSET( "100.000 TESTS" ) );

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
        result += ((fc::uint128_low_bits(rsf) & 1) == 0) ? '0' : '1';
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

    witness::block_producer bp( get_chain_plugin() );
    unsigned int init_block_num = db->head_block_num();
    int miss_blocks = fc::minutes( 1 ).to_seconds() / HIVE_BLOCK_INTERVAL;
    auto witness = db->get_scheduled_witness( miss_blocks );
    auto block_time = db->get_slot_time( miss_blocks );
    GENERATE_BLOCK( bp, block_time , witness, init_account_priv_key );

    BOOST_CHECK_EQUAL( db->head_block_num(), init_block_num + 1 );
    BOOST_CHECK( db->head_block_time() == block_time );

    BOOST_TEST_MESSAGE( "Generating a block through fixture" );
    generate_block();

    BOOST_CHECK_EQUAL( db->head_block_num(), init_block_num + 2 );
    BOOST_CHECK( db->head_block_time() == block_time + HIVE_BLOCK_INTERVAL );
  }
  FC_LOG_AND_RETHROW();
}

BOOST_FIXTURE_TEST_CASE( hardfork_test, hived_fixture )
{
  try
  {
    try {

    ah_plugin_type* ah_plugin = nullptr;

    postponed_init(
      {
        config_line_t( { "plugin", { HIVE_ACCOUNT_HISTORY_ROCKSDB_PLUGIN_NAME } } ),
        config_line_t( { "shared-file-size", { std::to_string( 1024 * 1024 * shared_file_size_small ) } } )
      },
      &ah_plugin
    );

    init_account_pub_key = init_account_priv_key.get_public_key();

    generate_blocks( 2 );

    vest( HIVE_INIT_MINER_NAME, ASSET( "10.000 TESTS" ) );

    // Fill up the rest of the required miners
    for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
    {
      account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
      fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD );
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
    hive::protocol::operation last_op;
    fc::raw::unpack_from_vector(itr->serialized_op, last_op);

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
      fc::raw::unpack_from_vector(itr->serialized_op, processed_op);
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

    push_transaction( tx, init_account_priv_key );

    // Second transaction, tx minus op is 78 (one less byte for operation vector size)
    // We need a 88 byte op. We need a 22 character memo (1 byte for length) 55 = 32 (old op) + 55 + 1
    op.memo = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ123";
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, init_account_priv_key );

    generate_block();

    // The last transfer should have been delayed due to size
    auto head_block = get_block_reader().get_block_by_number( db->head_block_num() );
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
    SET_UP_FIXTURE( true /*REMOVE_DB_FILES*/, data_dir.path().string() );
    witness::block_producer bp( chain_plugin );

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

#define SET_UP_DATABASE( NAME, THREAD_POOL, APP, DATA_DIR_PATH, LOG_HARDFORKS ) \
  database NAME( APP ); \
  { \
  std::shared_ptr< hive::chain::block_storage_i > block_storage_ ## NAME = \
    hive::chain::block_storage_i::create_storage( LEGACY_SINGLE_FILE_BLOCK_LOG, APP, THREAD_POOL ); \
  sync_block_writer sbw_ ## NAME ( *(block_storage_ ## NAME . get()), NAME, APP ); \
  NAME.set_block_writer( &sbw_ ## NAME ); \
  open_test_database( NAME, *(block_storage_ ## NAME .get()), DATA_DIR_PATH, APP, LOG_HARDFORKS ); \
  }
  
BOOST_AUTO_TEST_CASE( safe_closing_database )
{
  try {
    appbase::application app;
    hive::chain::blockchain_worker_thread_pool thread_pool = hive::chain::blockchain_worker_thread_pool( app );
    fc::temp_directory data_dir( hive::utilities::temp_directory_path() );
    SET_UP_DATABASE( db, thread_pool, app, data_dir.path(), true )
  }
  FC_LOG_AND_RETHROW()
}

template <class BASE>
class test_block_flow_control : public BASE
{
public:
  enum expectation
  {
    START, WRITE_QUEUE_POP, FORK_DB_INSERT,
    FORK_APPLY, FORK_IGNORE, FORK_NORMAL,
    END_OF_TXS, END_OF_BLOCK, END_PROCESSING, END,
    FAILURE
  };

  using BASE::BASE;
  using typename BASE::phase;
  virtual ~test_block_flow_control() = default;

  void expect( expectation x ) { current_expectations.emplace_back( x ); }
  void reset_expectations() { current_expectations.clear(); }
  void verify( phase p ) const
  {
    failure |= ( p != this->current_phase );
    BOOST_CHECK_EQUAL( p, this->current_phase );
  }
  void verify( expectation e ) const
  {
    failure |= current_expectations.empty();
    BOOST_REQUIRE( !current_expectations.empty() );
    expectation expected = current_expectations.front();
    current_expectations.pop_front();
    failure |= ( e != expected );
    BOOST_CHECK_EQUAL( e, expected );
  }
  void check_empty() const
  {
    BOOST_CHECK( !failure && current_expectations.empty() );
  }

  virtual void on_write_queue_pop( uint32_t _inc_txs, uint32_t _ok_txs, uint32_t _fail_auth, uint32_t _fail_no_rc ) const override
  {
    verify( phase::REQUEST );
    BOOST_CHECK( !this->except );
    BASE::on_write_queue_pop( _inc_txs, _ok_txs, _fail_auth, _fail_no_rc );
    verify( expectation::WRITE_QUEUE_POP );
  }

  virtual void on_fork_db_insert() const override
  {
    verify( phase::START );
    BOOST_CHECK( !this->except );
    BASE::on_fork_db_insert();
    verify( expectation::FORK_DB_INSERT );
  }
  virtual void on_fork_apply() const override
  {
    verify( phase::FORK_DB );
    BOOST_CHECK( !this->except );
    BASE::on_fork_apply();
    verify( expectation::FORK_APPLY );
  }
  virtual void on_fork_ignore() const override
  {
    verify( phase::FORK_DB );
    BOOST_CHECK( !this->except );
    BASE::on_fork_ignore();
    verify( expectation::FORK_IGNORE );
  }
  virtual void on_fork_normal() const override
  {
    verify( phase::FORK_DB );
    BOOST_CHECK( !this->except );
    BASE::on_fork_normal();
    verify( expectation::FORK_NORMAL );
  }
  virtual void on_end_of_transactions() const override
  {
    if( this->current_phase == phase::FORK_APPLY )
      verify( phase::FORK_APPLY );
    else
      verify( phase::FORK_NORMAL );
    BOOST_CHECK( !this->except );
    BASE::on_end_of_transactions();
    verify( expectation::END_OF_TXS );
  }
  virtual void on_end_of_apply_block() const override
  {
    if( this->current_phase == phase::FORK_IGNORE )
      verify( phase::FORK_IGNORE );
    else
      verify( phase::TXS_EXECUTED );
    BOOST_CHECK( !this->except );
    BASE::on_end_of_apply_block();
    verify( expectation::END_OF_BLOCK );
  }

  virtual void on_end_of_processing( uint32_t _exp_txs, uint32_t _fail_txs, uint32_t _ok_txs, uint32_t _post_txs, uint32_t _drop_txs, size_t _mempool_size, uint32_t _lib ) const override
  {
    // pending transactions are actually reapplied even after failed block and that is before exception
    // is passed to the block flow control; it means we can't be sure on what is current phase
    BASE::on_end_of_processing( _exp_txs, _fail_txs, _ok_txs, _post_txs, _drop_txs, _mempool_size, _lib );
    verify( expectation::END_PROCESSING );
  }

  virtual void on_failure( const fc::exception& e ) const override
  {
    BOOST_CHECK( !this->except ); //another exception notification should not happen during exception handling
    BASE::on_failure( e );
    verify( expectation::FAILURE );
  }
  virtual void on_worker_done( appbase::application& app ) const override
  {
    if( !this->except ) //called even in case of block failure
      verify( phase::END );
    BASE::on_worker_done( app );
    verify( expectation::END );
  }

private:
  mutable std::list< expectation > current_expectations;
  mutable bool failure = false;
};

BOOST_FIXTURE_TEST_CASE( block_flow_control_generation, clean_database_fixture )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing block flow during generation" );

    witness::block_producer bp( get_chain_plugin() );

    auto init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash( string( "init_key" ) ) );

    test_block_flow_control<generate_block_flow_control> bfc( db->get_slot_time(1),
      db->get_scheduled_witness(1), init_account_priv_key, database::skip_nothing );

    bfc.expect( bfc.WRITE_QUEUE_POP );
    bfc.expect( bfc.FORK_DB_INSERT );
    bfc.expect( bfc.FORK_NORMAL );
    bfc.expect( bfc.END_OF_TXS );
    bfc.expect( bfc.END_OF_BLOCK );
    bfc.expect( bfc.END_PROCESSING );
    bfc.expect( bfc.END );

    // since we are not running through full regular path (through witness and chain plugin)
    // we have to manually call those phases (we can't just skip expectations because phase tracking
    // will not work correctly)
    bfc.on_write_queue_pop( 0, 0, 0, 0 ); //normally called by chain plugin
    bp.generate_block( &bfc );
    bfc.on_worker_done( theApp ); //normally called by chain plugin
    bfc.check_empty();
  }
  FC_LOG_AND_RETHROW()
}

class test_promise : public fc::promise<void>
{
public:
  test_promise( const database& _db, const block_flow_control& _bfc )
    : fc::promise<void>("test"), db(_db), bfc( _bfc )
  {
    on_complete( [this]( const fc::exception_ptr& e ){
      if( !bfc.ignored() && !bfc.get_exception() )
        BOOST_CHECK_EQUAL( db.get_processed_block_id(), bfc.get_full_block()->get_block_id() );
      fulfilled = true;
    } );
  }

  virtual ~test_promise() { BOOST_CHECK( fulfilled ); }

private:
  const database& db;
  const block_flow_control& bfc;
  bool fulfilled = false;
};

BOOST_AUTO_TEST_CASE( block_flow_control_p2p )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing block flow during p2p block push" );

    fc::temp_directory data_dir_bp1( hive::utilities::temp_directory_path() );
    SET_UP_FIXTURE_SUFFIX( data_dir_bp1.path().string(), _bp1, fc::optional< fc::logging_config >() );
    auto common_logging_config = fixture_bp1.get_logging_config();
    witness::block_producer bp1( chain_plugin_bp1 );

    fc::temp_directory data_dir_bp2( hive::utilities::temp_directory_path() );
    SET_UP_FIXTURE_SUFFIX( data_dir_bp2.path().string(), _bp2, common_logging_config );
    witness::block_producer bp2( chain_plugin_bp2 );

    fc::temp_directory data_dir_( hive::utilities::temp_directory_path() );
    SET_UP_FIXTURE_SUFFIX( data_dir_.path().string(), _, common_logging_config );

    auto report_block = []( const database& db, const std::shared_ptr<full_block_type>& block, const char* suffix )
    {
      ilog( "block #${b} (${id}) with ts ${t} ${suffix}", ( "b", block->get_block_num() )
        ( "id", block->get_block_id() )( "t", block->get_block_header().timestamp )( suffix ) );
      ilog( "head is #${b} (${id})", ( "b", db.head_block_num() )( "id", db.head_block_id() ) );
    };

    auto init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash( string( "init_key" ) ) );
    auto block = GENERATE_BLOCK( bp1, db_bp1.get_slot_time( 1 ), db_bp1.get_scheduled_witness( 1 ),
      init_account_priv_key, database::skip_nothing );
    report_block( db_bp1, block, "generated by BP1" );

    PUSH_BLOCK( chain_plugin_bp2, block );
    report_block( db_bp2, block, "pushed to BP2" );

    {
      test_block_flow_control<p2p_block_flow_control> bfc( block, database::skip_nothing );
      bfc.expect( bfc.WRITE_QUEUE_POP );
      bfc.expect( bfc.FORK_DB_INSERT );
      bfc.expect( bfc.FORK_NORMAL );
      bfc.expect( bfc.END_OF_TXS );
      bfc.expect( bfc.END_OF_BLOCK );
      bfc.expect( bfc.END_PROCESSING );
      bfc.expect( bfc.END );
      fc::promise<void>::ptr promise( new test_promise( db_, bfc ) );
      bfc.attach_promise( promise );

      // since we are not running through full regular path (through p2p and chain plugin)
      // we have to manually call those phases (we can't just skip expectations because phase tracking
      // will not work correctly)
      bfc.on_write_queue_pop( 0, 0, 0, 0 ); //normally called by chain plugin
      chain_plugin_.push_block( bfc );
      bfc.on_worker_done( fixture_.theApp ); //normally called by chain plugin
      bfc.check_empty();
      report_block( db_, block, "pushed to DB" );
    }

    // produce couple more blocks and push to all databases
    for( int i = 1; i < 3; ++i )
    {
      block = GENERATE_BLOCK( bp1, db_bp1.get_slot_time( 1 ), db_bp1.get_scheduled_witness( 1 ),
        init_account_priv_key, database::skip_nothing );
      report_block( db_bp1, block, "generated by BP1" );
      PUSH_BLOCK( chain_plugin_bp2, block );
      report_block( db_bp2, block, "pushed to BP2" );
      PUSH_BLOCK( chain_plugin_, block );
      report_block( db_, block, "pushed to DB" );
    }

    {
      custom_json_operation op;
      op.required_posting_auths.emplace( HIVE_INIT_MINER_NAME );
      op.id = "test";
      op.json = "{}";
      signed_transaction tx;
      tx.set_expiration( db_bp1.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );
      auto pack = serialization_mode_controller::get_current_pack();
      full_transaction_ptr ftx = full_transaction_type::create_from_signed_transaction( tx, pack, false );
      ftx->sign_transaction( std::vector<fc::ecc::private_key>{ init_account_priv_key }, db_bp1.get_chain_id(), pack );
      // push transaction only to one database, so other one can produce different block
      chain_plugin_bp1.push_transaction( ftx, database::skip_nothing );
    }

    auto block1 = GENERATE_BLOCK( bp1, db_bp1.get_slot_time( 1 ), db_bp1.get_scheduled_witness( 1 ),
      init_account_priv_key, database::skip_nothing );
    report_block( db_bp1, block1, "generated by BP1" );
    PUSH_BLOCK( chain_plugin_, block1 );
    report_block( db_, block1, "pushed to DB" );

    BOOST_TEST_MESSAGE( "Testing block flow during duplicate block push" );
    // normally it should never happen because p2p only asks for the block from one peer, however
    // there is still dual block_message mechanism from pre-HF26 times which might cause the same
    // block to arrive twice (HF26+ compatible nodes should never use ond block_message id, but there
    // is possibility of syncing with pre-HF26 node too)

    {
      test_block_flow_control<p2p_block_flow_control> bfc( block1, database::skip_nothing );
      bfc.expect( bfc.WRITE_QUEUE_POP );
      bfc.expect( bfc.FORK_DB_INSERT );
      bfc.expect( bfc.FORK_IGNORE );
      bfc.expect( bfc.END_OF_BLOCK );
      bfc.expect( bfc.END_PROCESSING );
      bfc.expect( bfc.END );
      fc::promise<void>::ptr promise( new test_promise( db_, bfc ) );
      bfc.attach_promise( promise );

      bfc.on_write_queue_pop( 0, 0, 0, 0 ); //normally called by chain plugin
      chain_plugin_.push_block( bfc );
      bfc.on_worker_done( fixture_.theApp ); //normally called by chain plugin
      bfc.check_empty();
      report_block( db_, block1, "pushed to DB" );
    }

    auto block2 = GENERATE_BLOCK( bp2, db_bp2.get_slot_time( 1 ), db_bp2.get_scheduled_witness( 1 ),
      init_account_priv_key, database::skip_nothing );
    report_block( db_bp2, block2, "generated by BP2" );

    BOOST_TEST_MESSAGE( "Testing block flow during double produced block push" );

    {
      test_block_flow_control<p2p_block_flow_control> bfc( block2, database::skip_nothing );
      bfc.expect( bfc.WRITE_QUEUE_POP );
      bfc.expect( bfc.FORK_DB_INSERT );
      bfc.expect( bfc.FORK_IGNORE );
      bfc.expect( bfc.END_OF_BLOCK );
      bfc.expect( bfc.END_PROCESSING );
      bfc.expect( bfc.END );
      fc::promise<void>::ptr promise( new test_promise( db_, bfc ) );
      bfc.attach_promise( promise );

      bfc.on_write_queue_pop( 0, 0, 0, 0 ); //normally called by chain plugin
      chain_plugin_.push_block( bfc );
      bfc.on_worker_done( fixture_.theApp ); //normally called by chain plugin
      bfc.check_empty();
      report_block( db_, block2, "pushed to DB" );
    }

    PUSH_BLOCK( chain_plugin_bp2, block1 );
    report_block( db_bp2, block1, "pushed to BP2" );
    // db_bp1: block1 as main fork, block2 not present
    // db_bp2: block2 as main fork, block1 as ignored alternative
    // db: block1 as main fork, block2 as ignored alternative

    block = block2; // save that block aside - it will be needed later
    block2 = GENERATE_BLOCK( bp2, db_bp2.get_slot_time( 1 ), db_bp2.get_scheduled_witness( 1 ),
      init_account_priv_key, database::skip_nothing );
    report_block( db_bp2, block2, "generated by BP2" );

    BOOST_TEST_MESSAGE( "Testing block flow during regular fork" );

    {
      test_block_flow_control<p2p_block_flow_control> bfc( block2, database::skip_nothing );
      bfc.expect( bfc.WRITE_QUEUE_POP );
      bfc.expect( bfc.FORK_DB_INSERT );
      bfc.expect( bfc.FORK_APPLY );
      bfc.expect( bfc.END_OF_TXS );
      bfc.expect( bfc.END_OF_BLOCK );
      bfc.expect( bfc.END_PROCESSING );
      bfc.expect( bfc.END );
      fc::promise<void>::ptr promise( new test_promise( db_, bfc ) );
      bfc.attach_promise( promise );

      bfc.on_write_queue_pop( 0, 0, 0, 0 ); //normally called by chain plugin
      chain_plugin_.push_block( bfc );
      bfc.on_worker_done( fixture_.theApp ); //normally called by chain plugin
      bfc.check_empty();
      report_block( db_, block2, "pushed to DB" );
    }

    BOOST_TEST_MESSAGE( "Testing block flow during unlinkable block push" );

    {
      test_block_flow_control<p2p_block_flow_control> bfc( block2, database::skip_nothing );
      bfc.expect( bfc.WRITE_QUEUE_POP );
      bfc.expect( bfc.END_PROCESSING );
      bfc.expect( bfc.FAILURE );
      bfc.expect( bfc.END );
      fc::promise<void>::ptr promise( new test_promise( db_bp1, bfc ) );
      bfc.attach_promise( promise );

      bfc.on_write_queue_pop( 0, 0, 0, 0 ); //normally called by chain plugin
      try
      {
        chain_plugin_bp1.push_block( bfc );
      }
      catch( const unlinkable_block_exception& ex )
      {
        bfc.on_failure( ex ); //normally called by chain plugin
      }
      bfc.on_worker_done( fixture_bp1.theApp ); //normally called by chain plugin
      bfc.check_empty();
      report_block( db_bp1, block2, "pushed to BP1" );
      // now db_bp1 is still on its own fork but has block2 as unlinked item
    }

    BOOST_TEST_MESSAGE( "Testing block flow during regular fork with out of order block" );

    {
      // we are now applying previously saved block so the block2 that is unlinked in db_bp1 becomes linked
      // and causes new fork to be longer than current one for that node
      test_block_flow_control<p2p_block_flow_control> bfc( block, database::skip_nothing );
      bfc.expect( bfc.WRITE_QUEUE_POP );
      bfc.expect( bfc.FORK_DB_INSERT );
      bfc.expect( bfc.FORK_APPLY );
      bfc.expect( bfc.END_OF_TXS );
      bfc.expect( bfc.END_OF_BLOCK );
      bfc.expect( bfc.END_PROCESSING );
      bfc.expect( bfc.END );
      fc::promise<void>::ptr promise( new test_promise( db_bp1, bfc ) );
      bfc.attach_promise( promise );

      bfc.on_write_queue_pop( 0, 0, 0, 0 ); //normally called by chain plugin
      chain_plugin_bp1.push_block( bfc );
      bfc.on_worker_done( fixture_bp1.theApp ); //normally called by chain plugin
      bfc.check_empty();
      report_block( db_bp1, block, "pushed to BP1" );
    }

    // all three nodes are now on the same fork

    block1 = GENERATE_BLOCK( bp1, db_bp1.get_slot_time( 1 ), db_bp1.get_scheduled_witness( 1 ),
      init_account_priv_key, database::skip_nothing );
    report_block( db_bp1, block1, "generated by BP1" );
    PUSH_BLOCK( chain_plugin_bp2, block1 );
    report_block( db_bp2, block1, "pushed to BP2" );
    block2 = GENERATE_BLOCK( bp2, db_bp2.get_slot_time( 1 ), db_bp2.get_scheduled_witness( 1 ),
      init_account_priv_key, database::skip_nothing );
    report_block( db_bp2, block2, "generated by BP2" );
    PUSH_BLOCK( chain_plugin_bp1, block2 );
    report_block( db_bp1, block2, "pushed to BP1" );
    block = block1; // save that block aside - it will be needed later
    block1 = GENERATE_BLOCK( bp1, db_bp1.get_slot_time( 1 ), db_bp1.get_scheduled_witness( 1 ),
      init_account_priv_key, database::skip_nothing );
    report_block( db_bp1, block1, "generated by BP1" );
    PUSH_BLOCK( chain_plugin_bp2, block1 );
    report_block( db_bp2, block1, "pushed to BP2" );

    // db is missing block -> block2 -> block1; let's put them in but in reverse order

    try
    {
      PUSH_BLOCK( chain_plugin_, block1 );
    }
    catch( const unlinkable_block_exception& ex )
    {
      report_block( db_, block1, "pushed to DB" );
    }

    BOOST_TEST_MESSAGE( "Testing block flow during unlinkable block push that can form link with other unlinkable" );

    {
      test_block_flow_control<p2p_block_flow_control> bfc( block2, database::skip_nothing );
      bfc.expect( bfc.WRITE_QUEUE_POP );
      bfc.expect( bfc.END_PROCESSING );
      bfc.expect( bfc.FAILURE );
      bfc.expect( bfc.END );
      fc::promise<void>::ptr promise( new test_promise( db_, bfc ) );
      bfc.attach_promise( promise );

      bfc.on_write_queue_pop( 0, 0, 0, 0 ); //normally called by chain plugin
      try
      {
        chain_plugin_.push_block( bfc );
      }
      catch( const unlinkable_block_exception& ex )
      {
        bfc.on_failure( ex ); //normally called by chain plugin
      }
      bfc.on_worker_done( fixture_.theApp ); //normally called by chain plugin
      bfc.check_empty();
      report_block( db_, block2, "pushed to DB" );
      // now db still have not moved, but has two blocks already that could form a link
    }

    BOOST_TEST_MESSAGE( "Testing block flow during normal block push with out of order block" );

    {
      test_block_flow_control<p2p_block_flow_control> bfc( block, database::skip_nothing );
      bfc.expect( bfc.WRITE_QUEUE_POP );
      bfc.expect( bfc.FORK_DB_INSERT );
      bfc.expect( bfc.FORK_NORMAL );
      bfc.expect( bfc.END_OF_TXS );
      bfc.expect( bfc.END_OF_BLOCK );
      bfc.expect( bfc.END_PROCESSING );
      bfc.expect( bfc.END );
      fc::promise<void>::ptr promise( new test_promise( db_, bfc ) );
      bfc.attach_promise( promise );

      bfc.on_write_queue_pop( 0, 0, 0, 0 ); //normally called by chain plugin
      chain_plugin_.push_block( bfc );
      bfc.on_worker_done( fixture_.theApp ); //normally called by chain plugin
      bfc.check_empty();
      report_block( db_, block, "pushed to DB" );
    }

    // check if all nodes are on the same fork
    BOOST_CHECK_EQUAL( db_.head_block_id(), db_bp1.head_block_id() );
    BOOST_CHECK_EQUAL( db_.head_block_id(), db_bp2.head_block_id() );
  }
  FC_LOG_AND_RETHROW()
}

struct init_supply_database_fixture : public hived_fixture
{
  init_supply_database_fixture()
  {
    try
    {
      postponed_init( {
        config_line_t( { "shared-file-size", { std::to_string( 1024 * 1024 * shared_file_size_small ) } } )
      } );
      init_account_pub_key = init_account_priv_key.get_public_key();
      validate_database();
    }
    catch( const fc::exception& e )
    {
      edump( ( e.to_detail_string() ) );
      throw;
    }
  }
  virtual ~init_supply_database_fixture() {}
};

BOOST_FIXTURE_TEST_CASE( init_hive_hbd_supply, init_supply_database_fixture )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing transfer to null from initial supply" );

    // the test replicates issue #574 on HBD, for completeness it does similar checks on initial HIVE supply

    // default configuration for testnet includes some HIVE/HBD, for mainnet the value is 0 - genesis puts that supply on balance of 'initminer'
    BOOST_CHECK_EQUAL( get_balance( HIVE_INIT_MINER_NAME ).amount.value, HIVE_INIT_SUPPLY );
    BOOST_CHECK_EQUAL( get_hbd_balance( HIVE_INIT_MINER_NAME ).amount.value, HIVE_HBD_INIT_SUPPLY );
    // current HBD supply as well as virtual supply must include those values
    const auto& dgpo = db->get_dynamic_global_properties();
    BOOST_CHECK_EQUAL( dgpo.get_current_supply().amount.value, HIVE_INIT_SUPPLY );
    BOOST_CHECK_EQUAL( dgpo.get_current_hbd_supply().amount.value, HIVE_HBD_INIT_SUPPLY );
    BOOST_CHECK_EQUAL( dgpo.virtual_supply.amount.value, HIVE_INIT_SUPPLY + HIVE_HBD_INIT_SUPPLY ); //initial HIVE price is 1-1 with HBD

    // 'null' account balances are cleared only starting at HF14 and supply checks, that were the assertion
    // raised as result of the, bug are performed since HF20
    generate_block();
    db->set_hardfork( HIVE_NUM_HARDFORKS );
    generate_block();

    fund( HIVE_NULL_ACCOUNT, ASSET( "100.000 TESTS" ) );
    fund( HIVE_NULL_ACCOUNT, ASSET( "100.000 TBD" ) );

    generate_block(); // the bug caused assertion during reapplication of block containing HBD transfer to 'null'

    BOOST_CHECK_EQUAL( get_balance( HIVE_INIT_MINER_NAME ).amount.value, HIVE_INIT_SUPPLY - 100'000);
    BOOST_CHECK_EQUAL( get_hbd_balance( HIVE_INIT_MINER_NAME ).amount.value, HIVE_HBD_INIT_SUPPLY - 100'000 );
    // application of block burns HIVE/HBD on 'null' balances but block production adds inflation to
    // HIVE and HBD global balances (HBD goes to treasury)
    BOOST_CHECK_GT( dgpo.get_current_supply().amount.value, HIVE_INIT_SUPPLY - 100'000 );
    BOOST_CHECK_GT( dgpo.get_current_hbd_supply().amount.value, HIVE_HBD_INIT_SUPPLY - 100'000 );
    BOOST_CHECK_GT( dgpo.virtual_supply.amount.value, HIVE_INIT_SUPPLY + HIVE_HBD_INIT_SUPPLY - 200'000 );

    validate_database(); // fix for the bug included change in validate_invariants() called here
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif

#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/util/reward.hpp>

#include <hive/plugins/debug_node/debug_node_plugin.hpp>

#include <fc/crypto/digest.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

#include <cmath>

using namespace hive;
using namespace hive::chain;
using namespace hive::chain::util;
using namespace hive::protocol;

BOOST_FIXTURE_TEST_SUITE( accounts_in_external_storage, clean_database_fixture )

BOOST_AUTO_TEST_CASE( basic_checks )
{
  try
  {
    ACTORS( (alice)(bob)(sender) )

    vest( "alice", ASSET( "200.000 TESTS" ) );
    vest( "bob", ASSET( "200.000 TESTS" ) );
    vest( "sender", ASSET( "200.000 TESTS" ) );

    issue_funds( "alice", ASSET( "100.000 TESTS" ) );
    issue_funds( "bob", ASSET( "100.000 TESTS" ) );
    issue_funds( "sender", ASSET( "100.000 TESTS" ) );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    auto _trx_comment_with_bad_transfer = [&]()
    {
      signed_transaction _tx;

      transfer_operation _transfer_basic_00_op;
      _transfer_basic_00_op.from = "sender";
      _transfer_basic_00_op.to = "alice";
      _transfer_basic_00_op.amount = ASSET( "7.000 TESTS" );

      transfer_operation _transfer_basic_01_op;
      _transfer_basic_01_op.from = "sender";
      _transfer_basic_01_op.to = "bob";
      _transfer_basic_01_op.amount = ASSET( "7.000 TESTS" );

      comment_operation _comment_op;
      _comment_op.author = "alice";
      _comment_op.permlink = "my-permlink";
      _comment_op.parent_permlink = "my-permlink";
      _comment_op.title = "my-title";
      _comment_op.body = "my-body";

      transfer_operation _transfer_op;
      _transfer_op.from = "alice";
      _transfer_op.to = "bob";
      _transfer_op.amount = ASSET( "500.000 TESTS" );

      _tx.operations.push_back( _comment_op );
      _tx.operations.push_back( _transfer_op );

      _tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

      HIVE_REQUIRE_THROW( push_transaction( _tx, { alice_post_key, alice_private_key } ), fc::exception );
    };

    auto _trx_comment_with_good_transfer = [&]()
    {
      signed_transaction _tx;

      comment_operation _comment_op;
      _comment_op.author = "alice";
      _comment_op.permlink = "my-permlink";
      _comment_op.parent_permlink = "my-permlink";
      _comment_op.title = "my-title";
      _comment_op.body = "my-body";

      transfer_operation _transfer_op;
      _transfer_op.from = "alice";
      _transfer_op.to = "bob";
      _transfer_op.amount = ASSET( "0.001 TESTS" );

      _tx.operations.push_back( _comment_op );
      _tx.operations.push_back( _transfer_op );

      _tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

      push_transaction( _tx, { alice_post_key, alice_private_key } );
    };

    generate_blocks( 1 );

    _trx_comment_with_bad_transfer();
    generate_blocks( 1 );

    _trx_comment_with_good_transfer();
    generate_blocks( 1 );

    const auto& _alice = db->get_account( "alice" );
    const auto& _bob = db->get_account( "bob" );

    BOOST_REQUIRE_EQUAL( _alice.get_balance().amount.value, 99999 );
    BOOST_REQUIRE_EQUAL( _bob.get_balance().amount.value, 100001 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif

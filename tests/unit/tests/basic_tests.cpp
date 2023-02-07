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

#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/database.hpp>
#include <hive/protocol/protocol.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/transaction_object.hpp>

#include <hive/chain/util/reward.hpp>

#include <hive/chain/util/decoded_types_data_storage.hpp>
#include <hive/chain/pending_required_action_object.hpp>
#include <hive/chain/pending_optional_action_object.hpp>
#include <hive/plugins/block_log_info/block_log_info_objects.hpp>
#include <hive/plugins/account_by_key/account_by_key_objects.hpp>
#include <hive/plugins/market_history/market_history_plugin.hpp>
#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/reputation/reputation_objects.hpp>
#include <hive/plugins/tags/tags_plugin.hpp>
#include <hive/plugins/witness/witness_plugin_objects.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/plugins/follow/follow_objects.hpp>

#ifdef HIVE_ENABLE_SMT

#include <hive/chain/smt_objects/smt_token_object.hpp>
#include <hive/chain/smt_objects/account_balance_object.hpp>
#include <hive/chain/smt_objects/nai_pool_object.hpp>
#include <hive/chain/smt_objects/smt_token_object.hpp>

#endif

#include <fc/crypto/digest.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/uint128.hpp>
#include <fc/reflect/typename.hpp>
#include <fc/static_variant.hpp>
#include <fc/optional.hpp>
#include "../db_fixture/database_fixture.hpp"

#include <algorithm>
#include <random>
#include <string>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;

namespace std
{

ostream& operator << (ostream& o, const fc::uint128_t& u)
  {
  return o << to_string(u);
  }

} // namespace std

namespace
{

struct dummy : public object< 1111, dummy >
  {
  CHAINBASE_OBJECT( dummy );
  };

struct __test_for_alignment
{
  char            m1;
  ::fc::uint128_t m2 = 2;
  int             m3;
  ::fc::uint128_t m4 = 4;
  char            m5;
};

}

namespace fc
{
template<> struct get_typename<__test_for_alignment> { static const char* name() { return "__test_for_alignment"; } };
}

BOOST_FIXTURE_TEST_SUITE( basic_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( fixed_string_verification )
{
  using hive::protocol::details::truncation_controller;

  try
  {
    BOOST_TEST_MESSAGE( "Testing: fixed_string verification" );

    {
      account_name_type::set_verify( false );

      transfer_operation op;
      op.from = "abcde-0123456789";
      op.to = "bob";
      op.memo = "Memo";
      op.amount = asset( 100, HIVE_SYMBOL );
      op.validate();
    }

    {
      account_name_type::set_verify( false );

      transfer_operation op;
      op.from = "abcde-0123456789xxx";
      op.to = "bob";
      op.memo = "Memo";
      op.amount = asset( 100, HIVE_SYMBOL );
      op.validate();
    }

    {
      account_name_type::set_verify( true );

      transfer_operation op;
      op.from = "abcde-0123456789";
      op.to = "bob";
      op.memo = "Memo";
      op.amount = asset( 100, HIVE_SYMBOL );

      op.validate();
    }

    {
      account_name_type::set_verify( true );

      transfer_operation op;

      auto _assign = [&op]()
      {
        op.from = "abcde-0123456789xxx";
      };
      HIVE_REQUIRE_ASSERT( _assign(), "in_len <= sizeof(data)" );

      account_name_type::set_verify( false );
    }

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( parse_size_test )
{
  BOOST_CHECK_THROW( fc::parse_size( "" ), fc::parse_error_exception );
  BOOST_CHECK_THROW( fc::parse_size( "k" ), fc::parse_error_exception );

  BOOST_CHECK_EQUAL( fc::parse_size( "0" ), 0u );
  BOOST_CHECK_EQUAL( fc::parse_size( "1" ), 1u );
  BOOST_CHECK_EQUAL( fc::parse_size( "2" ), 2u );
  BOOST_CHECK_EQUAL( fc::parse_size( "3" ), 3u );
  BOOST_CHECK_EQUAL( fc::parse_size( "4" ), 4u );

  BOOST_CHECK_EQUAL( fc::parse_size( "9" ),   9u );
  BOOST_CHECK_EQUAL( fc::parse_size( "10" ), 10u );
  BOOST_CHECK_EQUAL( fc::parse_size( "11" ), 11u );
  BOOST_CHECK_EQUAL( fc::parse_size( "12" ), 12u );

  BOOST_CHECK_EQUAL( fc::parse_size( "314159265"), 314159265u );
  BOOST_CHECK_EQUAL( fc::parse_size( "1k" ), 1024u );
  BOOST_CHECK_THROW( fc::parse_size( "1a" ), fc::parse_error_exception );
  BOOST_CHECK_EQUAL( fc::parse_size( "1kb" ), 1000u );
  BOOST_CHECK_EQUAL( fc::parse_size( "1MiB" ), 1048576u );
  BOOST_CHECK_EQUAL( fc::parse_size( "32G" ), 34359738368u );
}

/**
  * Verify that names are RFC-1035 compliant https://tools.ietf.org/html/rfc1035
  * https://github.com/cryptonomex/graphene/issues/15
  */
BOOST_AUTO_TEST_CASE( valid_name_test )
{
  BOOST_CHECK( !is_valid_account_name( "a" ) );
  BOOST_CHECK( !is_valid_account_name( "A" ) );
  BOOST_CHECK( !is_valid_account_name( "0" ) );
  BOOST_CHECK( !is_valid_account_name( "." ) );
  BOOST_CHECK( !is_valid_account_name( "-" ) );

  BOOST_CHECK( !is_valid_account_name( "aa" ) );
  BOOST_CHECK( !is_valid_account_name( "aA" ) );
  BOOST_CHECK( !is_valid_account_name( "a0" ) );
  BOOST_CHECK( !is_valid_account_name( "a." ) );
  BOOST_CHECK( !is_valid_account_name( "a-" ) );

  BOOST_CHECK( is_valid_account_name( "aaa" ) );
  BOOST_CHECK( !is_valid_account_name( "aAa" ) );
  BOOST_CHECK( is_valid_account_name( "a0a" ) );
  BOOST_CHECK( !is_valid_account_name( "a.a" ) );
  BOOST_CHECK( is_valid_account_name( "a-a" ) );

  BOOST_CHECK( is_valid_account_name( "aa0" ) );
  BOOST_CHECK( !is_valid_account_name( "aA0" ) );
  BOOST_CHECK( is_valid_account_name( "a00" ) );
  BOOST_CHECK( !is_valid_account_name( "a.0" ) );
  BOOST_CHECK( is_valid_account_name( "a-0" ) );

  BOOST_CHECK(  is_valid_account_name( "aaa-bbb-ccc" ) );
  BOOST_CHECK(  is_valid_account_name( "aaa-bbb.ccc" ) );

  BOOST_CHECK( !is_valid_account_name( "aaa,bbb-ccc" ) );
  BOOST_CHECK( !is_valid_account_name( "aaa_bbb-ccc" ) );
  BOOST_CHECK( !is_valid_account_name( "aaa-BBB-ccc" ) );

  BOOST_CHECK( !is_valid_account_name( "1aaa-bbb" ) );
  BOOST_CHECK( !is_valid_account_name( "-aaa-bbb-ccc" ) );
  BOOST_CHECK( !is_valid_account_name( ".aaa-bbb-ccc" ) );
  BOOST_CHECK( !is_valid_account_name( "/aaa-bbb-ccc" ) );

  BOOST_CHECK( !is_valid_account_name( "aaa-bbb-ccc-" ) );
  BOOST_CHECK( !is_valid_account_name( "aaa-bbb-ccc." ) );
  BOOST_CHECK( !is_valid_account_name( "aaa-bbb-ccc.." ) );
  BOOST_CHECK( !is_valid_account_name( "aaa-bbb-ccc/" ) );

  BOOST_CHECK( !is_valid_account_name( "aaa..bbb-ccc" ) );
  BOOST_CHECK( is_valid_account_name( "aaa.bbb-ccc" ) );
  BOOST_CHECK( is_valid_account_name( "aaa.bbb.ccc" ) );

  BOOST_CHECK(  is_valid_account_name( "aaa--bbb--ccc" ) );
  BOOST_CHECK( !is_valid_account_name( "xn--san-p8a.de" ) );
  BOOST_CHECK(  is_valid_account_name( "xn--san-p8a.dex" ) );
  BOOST_CHECK( !is_valid_account_name( "xn-san-p8a.de" ) );
  BOOST_CHECK(  is_valid_account_name( "xn-san-p8a.dex" ) );

  BOOST_CHECK(  is_valid_account_name( "this-label-has" ) );
  BOOST_CHECK( !is_valid_account_name( "this-label-has-more-than-63-char.act.ers-64-to-be-really-precise" ) );
  BOOST_CHECK( !is_valid_account_name( "none.of.these.labels.has.more.than-63.chars--but.still.not.valid" ) );
}

BOOST_AUTO_TEST_CASE( merkle_root )
{
  signed_block block;
  vector<full_transaction_ptr> tx;
  vector<full_transaction_ptr> tx2;
  vector<digest_type> t;
  const uint32_t num_tx = 10;

  for( uint32_t i=0; i<num_tx; i++ )
  {
    signed_transaction _tx;
    _tx.ref_block_prefix = i;
    tx.emplace_back( hive::chain::full_transaction_type::create_from_signed_transaction( _tx, hive::protocol::pack_type::legacy, false /* cache this transaction */) );
    t.push_back( tx.back()->get_merkle_digest() );
  }

  auto c = []( const digest_type& digest ) -> checksum_type
  {   return checksum_type::hash( digest );   };

  auto d = []( const digest_type& left, const digest_type& right ) -> digest_type
  {   return digest_type::hash( std::make_pair( left, right ) );   };

  BOOST_CHECK( full_block_type::compute_merkle_root( tx2 ) == checksum_type() );

  tx2.push_back( tx[0] );
  BOOST_CHECK( full_block_type::compute_merkle_root( tx2 ) ==
    c(t[0])
    );

  digest_type dA, dB, dC, dD, dE, dI, dJ, dK, dM, dN, dO;

  /****************
    *              *
    *   A=d(0,1)   *
    *      / \     *
    *     0   1    *
    *              *
    ****************/

  dA = d(t[0], t[1]);

  tx2.push_back( tx[1] );
  BOOST_CHECK( full_block_type::compute_merkle_root( tx2 ) == c(dA) );

  /*************************
    *                       *
    *         I=d(A,B)      *
    *        /        \     *
    *   A=d(0,1)      B=2   *
    *      / \        /     *
    *     0   1      2      *
    *                       *
    *************************/

  dB = t[2];
  dI = d(dA, dB);

  tx2.push_back( tx[2] );
  BOOST_CHECK( full_block_type::compute_merkle_root( tx2 ) == c(dI) );

  /***************************
    *                         *
    *       I=d(A,B)          *
    *        /    \           *
    *   A=d(0,1)   B=d(2,3)   *
    *      / \    /   \       *
    *     0   1  2     3      *
    *                         *
    ***************************
    */

  dB = d(t[2], t[3]);
  dI = d(dA, dB);

  tx2.push_back( tx[3] );
  BOOST_CHECK( full_block_type::compute_merkle_root( tx2 ) == c(dI) );

  /***************************************
    *                                     *
    *                  __M=d(I,J)__       *
    *                 /            \      *
    *         I=d(A,B)              J=C   *
    *        /        \            /      *
    *   A=d(0,1)   B=d(2,3)      C=4      *
    *      / \        / \        /        *
    *     0   1      2   3      4         *
    *                                     *
    ***************************************/

  dC = t[4];
  dJ = dC;
  dM = d(dI, dJ);

  tx2.push_back( tx[4] );
  BOOST_CHECK( full_block_type::compute_merkle_root( tx2 ) == c(dM) );

  /**************************************
    *                                    *
    *                 __M=d(I,J)__       *
    *                /            \      *
    *        I=d(A,B)              J=C   *
    *       /        \            /      *
    *  A=d(0,1)   B=d(2,3)   C=d(4,5)    *
    *     / \        / \        / \      *
    *    0   1      2   3      4   5     *
    *                                    *
    **************************************/

  dC = d(t[4], t[5]);
  dJ = dC;
  dM = d(dI, dJ);

  tx2.push_back( tx[5] );
  BOOST_CHECK( full_block_type::compute_merkle_root( tx2 ) == c(dM) );

  /***********************************************
    *                                             *
    *                  __M=d(I,J)__               *
    *                 /            \              *
    *         I=d(A,B)              J=d(C,D)      *
    *        /        \            /        \     *
    *   A=d(0,1)   B=d(2,3)   C=d(4,5)      D=6   *
    *      / \        / \        / \        /     *
    *     0   1      2   3      4   5      6      *
    *                                             *
    ***********************************************/

  dD = t[6];
  dJ = d(dC, dD);
  dM = d(dI, dJ);

  tx2.push_back( tx[6] );
  BOOST_CHECK( full_block_type::compute_merkle_root( tx2 ) == c(dM) );

  /*************************************************
    *                                               *
    *                  __M=d(I,J)__                 *
    *                 /            \                *
    *         I=d(A,B)              J=d(C,D)        *
    *        /        \            /        \       *
    *   A=d(0,1)   B=d(2,3)   C=d(4,5)   D=d(6,7)   *
    *      / \        / \        / \        / \     *
    *     0   1      2   3      4   5      6   7    *
    *                                               *
    *************************************************/

  dD = d(t[6], t[7]);
  dJ = d(dC, dD);
  dM = d(dI, dJ);

  tx2.push_back( tx[7] );
  BOOST_CHECK( full_block_type::compute_merkle_root( tx2 ) == c(dM) );

  /************************************************************************
    *                                                                      *
    *                             _____________O=d(M,N)______________      *
    *                            /                                   \     *
    *                  __M=d(I,J)__                                  N=K   *
    *                 /            \                              /        *
    *         I=d(A,B)              J=d(C,D)                 K=E           *
    *        /        \            /        \            /                 *
    *   A=d(0,1)   B=d(2,3)   C=d(4,5)   D=d(6,7)      E=8                 *
    *      / \        / \        / \        / \        /                   *
    *     0   1      2   3      4   5      6   7      8                    *
    *                                                                      *
    ************************************************************************/

  dE = t[8];
  dK = dE;
  dN = dK;
  dO = d(dM, dN);

  tx2.push_back( tx[8] );
  BOOST_CHECK( full_block_type::compute_merkle_root( tx2 ) == c(dO) );

  /************************************************************************
    *                                                                      *
    *                             _____________O=d(M,N)______________      *
    *                            /                                   \     *
    *                  __M=d(I,J)__                                  N=K   *
    *                 /            \                              /        *
    *         I=d(A,B)              J=d(C,D)                 K=E           *
    *        /        \            /        \            /                 *
    *   A=d(0,1)   B=d(2,3)   C=d(4,5)   D=d(6,7)   E=d(8,9)               *
    *      / \        / \        / \        / \        / \                 *
    *     0   1      2   3      4   5      6   7      8   9                *
    *                                                                      *
    ************************************************************************/

  dE = d(t[8], t[9]);
  dK = dE;
  dN = dK;
  dO = d(dM, dN);

  tx2.push_back( tx[9] );
  BOOST_CHECK( full_block_type::compute_merkle_root( tx2 ) == c(dO) );
}

BOOST_AUTO_TEST_CASE( adjust_balance_test )
{
  ACTORS( (alice) );

  generate_block();

  BOOST_TEST_MESSAGE( "Testing adjust_balance" );

  BOOST_TEST_MESSAGE( " --- Testing adding HIVE_SYMBOL" );
  db->adjust_balance( "alice", asset( 50000, HIVE_SYMBOL ) );
  BOOST_REQUIRE( db->get_balance( "alice", HIVE_SYMBOL ) == asset( 50000, HIVE_SYMBOL ) );

  BOOST_TEST_MESSAGE( " --- Testing deducting HIVE_SYMBOL" );
  HIVE_REQUIRE_THROW( db->adjust_balance( "alice", asset( -50001, HIVE_SYMBOL ) ), fc::assert_exception );
  db->adjust_balance( "alice", asset( -30000, HIVE_SYMBOL ) );
  db->adjust_balance( "alice", asset( -20000, HIVE_SYMBOL ) );
  BOOST_REQUIRE( db->get_balance( "alice", HIVE_SYMBOL ) == asset( 0, HIVE_SYMBOL ) );

  BOOST_TEST_MESSAGE( " --- Testing adding HBD_SYMBOL" );
  db->adjust_balance( "alice", asset( 100000, HBD_SYMBOL ) );
  BOOST_REQUIRE( db->get_balance( "alice", HBD_SYMBOL ) == asset( 100000, HBD_SYMBOL ) );

  BOOST_TEST_MESSAGE( " --- Testing deducting HBD_SYMBOL" );
  HIVE_REQUIRE_THROW( db->adjust_balance( "alice", asset( -100001, HBD_SYMBOL ) ), fc::assert_exception );
  db->adjust_balance( "alice", asset( -50000, HBD_SYMBOL ) );
  db->adjust_balance( "alice", asset( -25000, HBD_SYMBOL ) );
  db->adjust_balance( "alice", asset( -25000, HBD_SYMBOL ) );
  BOOST_REQUIRE( db->get_balance( "alice", HBD_SYMBOL ) == asset( 0, HBD_SYMBOL ) );
}

BOOST_AUTO_TEST_CASE( curation_weight_test )
{
  fc::uint128_t rshares = 856158;
  fc::uint128_t s = fc::to_uint128( 0, 2000000000000ull );
  fc::uint128_t sqrt = fc::uint128_approx_sqrt( rshares + 2 * s );
  uint64_t result = fc::uint128_to_uint64( rshares / sqrt );

  BOOST_REQUIRE( fc::uint128_to_uint64(sqrt) == 2002250 );
  BOOST_REQUIRE( result == 0 );

  rshares = 0;
  sqrt = fc::uint128_approx_sqrt( rshares + 2 * s );
  result = fc::uint128_to_uint64( rshares / sqrt );

  BOOST_REQUIRE( fc::uint128_to_uint64(sqrt) == 2002250 );
  BOOST_REQUIRE( result == 0 );

  result = fc::uint128_to_uint64( uint128_t( 0 ) - uint128_t( 0 ) );

  BOOST_REQUIRE( result == 0 );
  rshares = fc::to_uint128( 0, 3351842535167ull );

  for( int64_t i = 856158; i >= 0; --i )
  {
    uint64_t old_weight = fc::uint128_to_uint64(util::evaluate_reward_curve( rshares - i, protocol::convergent_square_root, s ));
    uint64_t new_weight = fc::uint128_to_uint64(util::evaluate_reward_curve( rshares, protocol::convergent_square_root, s ));

    BOOST_REQUIRE( old_weight <= new_weight );

    uint128_t w( new_weight - old_weight );

    w *= 300;
    w /= 300;
    BOOST_REQUIRE( fc::uint128_to_uint64(w) == new_weight - old_weight );
  }

  //idump( (delta)(old_weight)(new_weight) );

}

BOOST_AUTO_TEST_CASE( fc_uint128_to_string )
{
  fc::uint128 fci(0xf);
  __uint128_t gcci(0xf);

  for (int i = 1; i < 129; ++i)
  {
    BOOST_CHECK_EQUAL( std::to_string(fci), std::to_string(gcci) );
    fci <<= 1;
    gcci <<= 1;
  }

  fci = 333;
  gcci = 333;

  for (int i = 1; i < 1001; ++i)
  {
    BOOST_CHECK_EQUAL( std::to_string(fci), std::to_string(gcci) );
    fci += 261; fci *= i;
    gcci += 261; gcci *= i;
  }

  for (int i = 1; i < 1001; ++i)
  {
    uint32_t i1 = std::rand();
    uint32_t i2 = std::rand();
    uint32_t i3 = std::rand();
    uint32_t i4 = std::rand();
    uint64_t lo = static_cast<uint64_t>(i1) << 32 | i2;
    uint64_t hi = static_cast<uint64_t>(i3) << 32 | i4;
    fci = fc::to_uint128(hi, lo);
    gcci = static_cast<__uint128_t>(hi) << 64 | lo;
    BOOST_CHECK_EQUAL( std::to_string(fci), std::to_string(gcci) );
  }

}

#ifndef ENABLE_STD_ALLOCATOR

BOOST_AUTO_TEST_CASE( chain_object_size )
{
  BOOST_CHECK_EQUAL( sizeof( dummy ), 4u );
  BOOST_CHECK_EQUAL( sizeof( util::manabar ), 16u );
  BOOST_CHECK_EQUAL( sizeof( fc::array<share_type, HIVE_MAX_PROXY_RECURSION_DEPTH> ), 32u );
  BOOST_CHECK_EQUAL( sizeof( t_vector< delayed_votes_data > ), 32u );

  //typical elements of various objects
  BOOST_CHECK_EQUAL( sizeof( account_object::id_type ), 4u ); //hidden first element of all objects (here just an example, all are the same size)
  BOOST_CHECK_EQUAL( sizeof( account_id_type ), 4u ); //all id_refs are of the same size
  BOOST_CHECK_EQUAL( sizeof( time_point_sec ), 4u );
  BOOST_CHECK_EQUAL( sizeof( share_type ), 8u );
  BOOST_CHECK_EQUAL( sizeof( HIVE_asset ), 8u ); //all tiny assets are of the same size
  BOOST_CHECK_EQUAL( sizeof( asset ), 16u );
  BOOST_CHECK_EQUAL( sizeof( account_name_type ), 16u );
  BOOST_CHECK_EQUAL( sizeof( shared_string ), 32u ); //it has dynamic component as well
  BOOST_CHECK_EQUAL( sizeof( price ), 32u );
  BOOST_CHECK_EQUAL( sizeof( t_vector< char > ), 32u ); //it has dynamic component as well, all vectors have the same static size
  BOOST_CHECK_EQUAL( sizeof( public_key_type ), 33u );
  BOOST_CHECK_EQUAL( sizeof( hive::protocol::fixed_string<16> ), 16u );
  BOOST_CHECK_EQUAL( sizeof( hive::protocol::fixed_string<24> ), 24u );
  BOOST_CHECK_EQUAL( sizeof( hive::protocol::fixed_string<32> ), 32u );
  BOOST_CHECK_EQUAL( sizeof( hive::protocol::custom_id_type ), 32u );
  BOOST_CHECK_EQUAL( alignof( hive::protocol::custom_id_type ), 16u );

  BOOST_CHECK_EQUAL( sizeof( hive::protocol::custom_json_operation ), 112u );
  BOOST_CHECK_EQUAL( alignof( hive::protocol::custom_json_operation ), 16u );
  BOOST_CHECK_EQUAL( offsetof( hive::protocol::custom_json_operation, id ), 48u );
  BOOST_CHECK_EQUAL( sizeof( hive::protocol::operation ), 352u );
  BOOST_CHECK_EQUAL( alignof( hive::protocol::operation ), 16u );

  /*
  The purpose of this test is to make you think about the impact on RAM when you make changes in chain objects.
  Also somewhat helps in catching new problems with alignment (f.e. when you added a flag member and object
  grew by 8 bytes it might be inevitable but it should prompt you to double check member placement).
  Finally, especially when you are adding new objects, you should think if there is a mechanism (resource cost or
  hard limit) that prevents use of the object creating operation in RAM attack.
  */

  //top RAM gluttons
  BOOST_CHECK_EQUAL( sizeof( comment_object ), 32u ); //85M+ growing fast
  BOOST_CHECK_EQUAL( sizeof( comment_index::MULTIINDEX_NODE_TYPE ), 96u );

  //permanent objects (no operation to remove)
  BOOST_TEST_MESSAGE("alignof(account_object): " << alignof(account_object));
  BOOST_CHECK_EQUAL(alignof(account_object), 16u);
  BOOST_CHECK_EQUAL( sizeof( account_object ), 432u ); //1.3M+
  BOOST_CHECK_EQUAL( sizeof( account_index::MULTIINDEX_NODE_TYPE ), 624u );
  BOOST_CHECK_EQUAL( sizeof( account_metadata_object ), 72u ); //as many as account_object, but only FatNode (also to be moved to HiveMind)
  BOOST_CHECK_EQUAL( sizeof( account_metadata_index::MULTIINDEX_NODE_TYPE ), 136u );
  BOOST_CHECK_EQUAL( sizeof( account_authority_object ), 248u ); //as many as account_object
  BOOST_CHECK_EQUAL( sizeof( account_authority_index::MULTIINDEX_NODE_TYPE ), 312u );
  BOOST_CHECK_EQUAL( sizeof( liquidity_reward_balance_object ), 48u ); //obsolete - only created/modified up to HF12 (683 objects)
  BOOST_CHECK_EQUAL( sizeof( liquidity_reward_balance_index::MULTIINDEX_NODE_TYPE ), 144u );
  BOOST_CHECK_EQUAL( sizeof( witness_object ), 352u ); //small but potentially as many as account_object
  BOOST_CHECK_EQUAL( sizeof( witness_index::MULTIINDEX_NODE_TYPE ), 544u );

  //lasting objects (operation to create and remove, but with potential to grow)
  BOOST_CHECK_EQUAL( sizeof( vesting_delegation_object ), 24u ); //1M+ (potential of account_object squared !!!)
  BOOST_CHECK_EQUAL( sizeof( vesting_delegation_index::MULTIINDEX_NODE_TYPE ), 88u );
  BOOST_CHECK_EQUAL( sizeof( withdraw_vesting_route_object ), 48u ); //45k (potential of 10*account_object)
  BOOST_CHECK_EQUAL( sizeof( withdraw_vesting_route_index::MULTIINDEX_NODE_TYPE ), 144u );
  BOOST_CHECK_EQUAL( sizeof( witness_vote_object ), 40u ); //450k (potential of 30*account_object)
  BOOST_CHECK_EQUAL( sizeof( witness_vote_index::MULTIINDEX_NODE_TYPE ), 136u );

  //buffered objects (operation to create, op/vop to remove after certain time)
  BOOST_CHECK_EQUAL( sizeof( transaction_object ), 28u ); //at most <1h> of transactions
  BOOST_CHECK_EQUAL( sizeof( transaction_index::MULTIINDEX_NODE_TYPE ), 128u );
  BOOST_CHECK_EQUAL( sizeof( vesting_delegation_expiration_object ), 24u ); //at most <5d> of undelegates
  BOOST_CHECK_EQUAL( sizeof( vesting_delegation_expiration_index::MULTIINDEX_NODE_TYPE ), 120u );
  BOOST_CHECK_EQUAL( sizeof( owner_authority_history_object ), 104u ); //at most <30d> of ownership updates
  BOOST_CHECK_EQUAL( sizeof( owner_authority_history_index::MULTIINDEX_NODE_TYPE ), 168u );
  BOOST_CHECK_EQUAL( sizeof( account_recovery_request_object ), 96u ); //at most <1d> of account recoveries
  BOOST_CHECK_EQUAL( sizeof( account_recovery_request_index::MULTIINDEX_NODE_TYPE ), 192u );
  BOOST_CHECK_EQUAL( sizeof( change_recovery_account_request_object ), 40u ); //at most <30d> of recovery account changes
  BOOST_CHECK_EQUAL( sizeof( change_recovery_account_request_index::MULTIINDEX_NODE_TYPE ), 136u );
  BOOST_CHECK_EQUAL( sizeof( comment_cashout_object ), 128u //at most <7d> of unpaid comments (all comments prior to HF19)
#ifdef HIVE_ENABLE_SMT
    + 32
#endif
  );
  BOOST_CHECK_EQUAL( sizeof( comment_cashout_index::MULTIINDEX_NODE_TYPE ), 192u
#ifdef HIVE_ENABLE_SMT
    + 32
#endif
  );
  BOOST_CHECK_EQUAL( sizeof( comment_cashout_ex_object ), 64u ); //all comments up to HF19, later not used
  BOOST_CHECK_EQUAL( sizeof( comment_cashout_ex_index::MULTIINDEX_NODE_TYPE ), 128u );
  BOOST_CHECK_EQUAL( sizeof( comment_vote_object ), 48u ); //at most <7d> of votes on unpaid comments
  BOOST_CHECK_EQUAL( sizeof( comment_vote_index::MULTIINDEX_NODE_TYPE ), 144u );
  BOOST_CHECK_EQUAL( sizeof( convert_request_object ), 24u ); //at most <3.5d> of conversion requests
  BOOST_CHECK_EQUAL( sizeof( convert_request_index::MULTIINDEX_NODE_TYPE ), 120u );
  BOOST_CHECK_EQUAL( sizeof( collateralized_convert_request_object ), 32u ); //at most <3.5d> of conversion requests
  BOOST_CHECK_EQUAL( sizeof( collateralized_convert_request_index::MULTIINDEX_NODE_TYPE ), 128u );
  BOOST_CHECK_EQUAL( sizeof( escrow_object ), 120u ); //small but potentially lasting forever, limited to 255*account_object
  BOOST_CHECK_EQUAL( sizeof( escrow_index::MULTIINDEX_NODE_TYPE ), 216u );
  BOOST_CHECK_EQUAL( sizeof( savings_withdraw_object ), 104u ); //at most <3d> of saving withdrawals
  BOOST_CHECK_EQUAL( sizeof( savings_withdraw_index::MULTIINDEX_NODE_TYPE ), 232u );
  BOOST_CHECK_EQUAL( sizeof( limit_order_object ), 80u ); //at most <28d> of limit orders
  BOOST_CHECK_EQUAL( sizeof( limit_order_index::MULTIINDEX_NODE_TYPE ), 208u );
  BOOST_CHECK_EQUAL( sizeof( decline_voting_rights_request_object ), 32u ); //at most <30d> of decline requests
  BOOST_CHECK_EQUAL( sizeof( decline_voting_rights_request_index::MULTIINDEX_NODE_TYPE ), 128u );
  BOOST_CHECK_EQUAL( sizeof( proposal_object ), 144u ); //potentially infinite, but costs a lot to make (especially after HF24)
  BOOST_CHECK_EQUAL( sizeof( proposal_index::MULTIINDEX_NODE_TYPE ), 336u );
  BOOST_CHECK_EQUAL( sizeof( proposal_vote_object ), 32u ); //potentially infinite, but limited by account_object and time of proposal_object life
  BOOST_CHECK_EQUAL( sizeof( proposal_vote_index::MULTIINDEX_NODE_TYPE ), 128u );
  BOOST_CHECK_EQUAL( sizeof( recurrent_transfer_object ), 72u ); //TODO: estimate number of active objects
  BOOST_CHECK_EQUAL( sizeof( recurrent_transfer_index::MULTIINDEX_NODE_TYPE ), 200u );

  //singletons (size only affects performance)
  BOOST_CHECK_EQUAL( sizeof( reward_fund_object ), 112u );
  BOOST_CHECK_EQUAL( sizeof( reward_fund_index::MULTIINDEX_NODE_TYPE ), 176u );
  BOOST_CHECK_EQUAL( sizeof( dynamic_global_property_object ), 384u
#ifdef HIVE_ENABLE_SMT
    + 16
#endif
  );
  BOOST_CHECK_EQUAL( sizeof( dynamic_global_property_index::MULTIINDEX_NODE_TYPE ), 416u
#ifdef HIVE_ENABLE_SMT
    + 16
#endif
  );
  BOOST_CHECK_EQUAL( sizeof( block_summary_object ), 24u ); //always 64k objects
  BOOST_CHECK_EQUAL( sizeof( block_summary_index::MULTIINDEX_NODE_TYPE ), 56u );
  BOOST_CHECK_EQUAL( sizeof( hardfork_property_object ), 120u );
  BOOST_CHECK_EQUAL( sizeof( hardfork_property_index::MULTIINDEX_NODE_TYPE ), 152u );
  BOOST_CHECK_EQUAL( sizeof( feed_history_object ), 232u ); //dynamic size worth 7*24 of sizeof(price)
  BOOST_CHECK_EQUAL( sizeof( feed_history_index::MULTIINDEX_NODE_TYPE ), 264u );
  BOOST_CHECK_EQUAL( sizeof( witness_schedule_object ), 544u );
  BOOST_CHECK_EQUAL( sizeof( witness_schedule_index::MULTIINDEX_NODE_TYPE ), 576u );

  //TODO: categorize and evaluate size potential of SMT related objects:
  //account_regular_balance_object
  //account_rewards_balance_object
  //nai_pool_object
  //smt_token_object
  //smt_ico_object
  //smt_token_emissions_object
  //smt_contribution_object

  //only used in tests, but open for use in theory:
  //pending_optional_action_object
  //pending_required_action_object

  BOOST_CHECK_EQUAL( sizeof( full_transaction_type ), 456 ); //not a chain object but potentially very numerous

}
#endif

BOOST_AUTO_TEST_CASE( fc_static_variant_alignment )
{
  BOOST_CHECK_EQUAL( sizeof(__test_for_alignment), 80u );
  BOOST_CHECK_EQUAL( alignof(__test_for_alignment), 16u );

  using test_static_variant = static_variant<int, char, __test_for_alignment>;

  BOOST_CHECK_EQUAL( alignof(test_static_variant), 16u );
  BOOST_CHECK_EQUAL( alignof(static_variant<int, char>), 8u ); //we could actually make it 4

  test_static_variant t[2];
  t[0] = __test_for_alignment();
  t[1] = __test_for_alignment();
  t[0].get<__test_for_alignment>().m2 = t[1].get<__test_for_alignment>().m4;
  t[0].get<__test_for_alignment>().m4 = t[1].get<__test_for_alignment>().m2;
  BOOST_CHECK_EQUAL( t[0].get<__test_for_alignment>().m2, t[1].get<__test_for_alignment>().m4 );
  BOOST_CHECK_EQUAL( t[0].get<__test_for_alignment>().m4, t[1].get<__test_for_alignment>().m2 );
}

BOOST_AUTO_TEST_CASE( fc_optional_alignment )
{
  using test_optional = fc::optional<__test_for_alignment>;

  BOOST_CHECK_EQUAL( alignof(test_optional), 16u );
  BOOST_CHECK_EQUAL( alignof(fc::optional<int>), 4u );

  test_optional t[2];
  t[0] = __test_for_alignment();
  t[1] = __test_for_alignment();
  t[0]->m2 = t[1]->m4;
  t[0]->m4 = t[1]->m2;
  BOOST_CHECK_EQUAL( t[0]->m2, t[1]->m4 );
  BOOST_CHECK_EQUAL( t[0]->m4, t[1]->m2 );
}

BOOST_AUTO_TEST_CASE( types_checksum_tests )
{
  hive::chain::util::decoded_types_data_storage& dtds_instance = hive::chain::util::decoded_types_data_storage::get_instance();

  /* comparing strings is more human readable if any error occurs */
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_object>().str(), fc::ripemd160("b8ee50dbb9ca8c00faccf1f2f550a4475252dd7f").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_metadata_object>().str(), fc::ripemd160("09569dbdd124e140fc7e7e5a6c3d1b1355f1a26a").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_authority_object>().str(), fc::ripemd160("532a9294e8cd92ac4ab5c085edf48ba3aca0d109").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::vesting_delegation_object>().str(), fc::ripemd160("8eedd68bc1cb33ae392727a1aa888fc87034ccf7").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::vesting_delegation_expiration_object>().str(), fc::ripemd160("bf17ef2da978f46c803ab43cf38449342b43c9e8").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::owner_authority_history_object>().str(), fc::ripemd160("8bd7a29bf7673162a915c0dac1c531afb803ed01").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_recovery_request_object>().str(), fc::ripemd160("7d5342fa60836aef4ca272cc18b9db0e2b2fc477").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::change_recovery_account_request_object>().str(), fc::ripemd160("24756a5c3c936357776ec9b8755d25e75cbbb2bc").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::block_summary_object>().str(), fc::ripemd160("6c2d3676c9bdbfc81ab70f0bfa8bde8c9ec0d9e3").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_object>().str(), fc::ripemd160("67cd2c24dcc7cf4e446b1f1ae217f6cfa3509a7c").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_cashout_ex_object>().str(), fc::ripemd160("c8565e604fbce0f27a9b17d0e65cce2dec3d9e6b").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_vote_object>().str(), fc::ripemd160("4453a5c3fe302390e78c4b8b116d2d0441be2000").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::proposal_object>().str(), fc::ripemd160("22b1e73fa49cb445df51569565061a3195188142").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::proposal_vote_object>().str(), fc::ripemd160("7b6c5d88f995f72dc64be32710cafad81b3e5f5c").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::hardfork_property_object>().str(), fc::ripemd160("f714e136c1a497c510727de00cac67a0ec34a3aa").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::convert_request_object>().str(), fc::ripemd160("0fe2b09ab79fb29b9ab0667973d4ce49d7d70fa0").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::collateralized_convert_request_object>().str(), fc::ripemd160("4d88c437d3a10c1666519511a2d90eff7d7cf8f4").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::escrow_object>().str(), fc::ripemd160("484bfbc9b556151f92a344a75a5221cf7b3d55d4").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::savings_withdraw_object>().str(), fc::ripemd160("4e7080dea8725be2f41137eedfcee99728b35fc4").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::liquidity_reward_balance_object>().str(), fc::ripemd160("56910482a09b2758b16d1deb4fe58ca068f9fe51").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::feed_history_object>().str(), fc::ripemd160("4be2773be237ccb89b0b676e138cfc3fd4033aac").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::limit_order_object>().str(), fc::ripemd160("a268d3f69f344bf7386d3b41afe6bda37ec9daaa").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::withdraw_vesting_route_object>().str(), fc::ripemd160("aee3ea260d706793ff6d47bb9a29af208a56b138").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::decline_voting_rights_request_object>().str(), fc::ripemd160("502cdba836acb5f18cc5240f523ce3835e2faa6b").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::reward_fund_object>().str(), fc::ripemd160("3bdc364231d34331e26ac1fa8b5b890bc7d19566").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::recurrent_transfer_object>().str(), fc::ripemd160("40610af476a030dd605d514797038ccc0f8b371c").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::pending_required_action_object>().str(), fc::ripemd160("030bcbe13b0b95ce23dcf4a13cb5a71ffd1283b9").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::pending_optional_action_object>().str(), fc::ripemd160("bce3e2e4f0bb6f5b6ad84d3e229315ec42d49d15").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::transaction_object>().str(), fc::ripemd160("b9eb5f401d0efd527efe1a3089c68e2064055249").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::witness_vote_object>().str(), fc::ripemd160("a83e61df4eceb1cbb06ed36c70b4379ef52012f4").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::witness_schedule_object>().str(), fc::ripemd160("68ec476ea8d8e106a276694f52d8a6f464021b6f").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::witness_object>().str(), fc::ripemd160("fd970cfd4115cb565f5f9d58a4786db02a1faa14").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::account_by_key::key_lookup_object>().str(), fc::ripemd160("b5661f6e9a969d23efe74b1632498994c85f449e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::account_history_rocksdb::volatile_operation_object>().str(), fc::ripemd160("ad71d8164cee165eb03e8d2534e9c288e669294e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::block_log_info::block_log_hash_state_object>().str(), fc::ripemd160("c710178345c7bcaa9f5a72b4cdf1dc45ce84f59d").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::block_log_info::block_log_pending_message_object>().str(), fc::ripemd160("6511c8e9a8a35415c7b6a136a2368e5371fb3f4d").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::follow_object>().str(), fc::ripemd160("9ec7d7cde097586afd499a187a3b49ce95a729de").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::feed_object>().str(), fc::ripemd160("f7e15b02a914b8264e294f071dd2e2fdec0b72e6").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::blog_object>().str(), fc::ripemd160("363cd22046d7cafba415c5ad783742ecc406823b").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::blog_author_stats_object>().str(), fc::ripemd160("2019ff7af579aff183a273e30ae7101959d6109e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::reputation_object>().str(), fc::ripemd160("6c13183396c4cdee423963530a3344116e780ae6").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::follow_count_object>().str(), fc::ripemd160("d88deef4e709bb4fab20afa17b5a25d603eccfae").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::market_history::order_history_object>().str(), fc::ripemd160("5eac9da9e4d6285f00504950b64f463bab90620e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_resource_param_object>().str(), fc::ripemd160("60167602314ac2fb78cbf5cd2c42dc743673376a").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_pool_object>().str(), fc::ripemd160("014f0f64dc054fb96c34062a98bbea4aba9b5117").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_pending_data>().str(), fc::ripemd160("d5e97974cd75493d90802ab01db5acd444b8acb4").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_account_object>().str(), fc::ripemd160("3c26d32439a0143c126346f2b4a8b6827fece7c2").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_direct_delegation_object>().str(), fc::ripemd160("c4f36c482e5fc6d63bd1f8a128f8510718f9a759").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_usage_bucket_object>().str(), fc::ripemd160("891e927cfce3c8780e2fc2fd31cc3235168273e0").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::reputation::reputation_object>().str(), fc::ripemd160("e6ca2e8a612c4b357340c7e37c7cecd03a8fc51f").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::tag_object>().str(), fc::ripemd160("fd017b495cdf4ff2e885735f8343329be25851df").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::tag_stats_object>().str(), fc::ripemd160("00342887e23a1b3a2721dfc8b1c3334bed7719ec").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::author_tag_stats_object>().str(), fc::ripemd160("ceeaeead6e51fdff8a7554daa61ee3db1d6d7b4e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::transaction_status::transaction_status_object>().str(), fc::ripemd160("447d1026c3cc02dc47f9afb2282184274cd5e830").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::witness::witness_custom_op_object>().str(), fc::ripemd160("001551883f460560a52593e66e0f2aa25f84369c").str() );

  #ifdef HIVE_ENABLE_SMT
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_token_object>().str(), fc::ripemd160("33766a0402eb5b98dc22fca7028825249427cb8b").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_regular_balance_object>().str(), fc::ripemd160("66f73d9770b58c753929ca4999eca94ce9d63164").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_rewards_balance_object>().str(), fc::ripemd160("5e7873e9a45aaac0e9be58d496808cf7af99062c").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::nai_pool_object>().str(), fc::ripemd160("89160725fec27b97bf4103b2376344b77e6ab907").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_token_emissions_object>().str(), fc::ripemd160("b438fb23a86a591dd27ddaa68a009aaad50d1a3e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_contribution_object>().str(), fc::ripemd160("6f78b583257989c260275cfb5e82f141fa447a83").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_ico_object>().str(), fc::ripemd160("c13787e44a827b3fafe487ae4d1e3e8219c8ba20").str() );

  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_cashout_object>().str(), fc::ripemd160("d6fa0377d1cc1f4436e29a25fd9be01f513f8be7").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::dynamic_global_property_object>().str(), fc::ripemd160("d6a50a17c1c3bec5cb9ffadcc86f2b7436b1ead8").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::market_history::bucket_object>().str(), fc::ripemd160("4dfaebaf00d594efafd1137845e108590a3abbc3").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_stats_object>().str(), fc::ripemd160("3d213282743e5f4da31b39e67a68b31479d78a70").str() );
  #else
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_cashout_object>().str(), fc::ripemd160("826b8b0ef2cded94a8e88427642abf0888edacbb").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::dynamic_global_property_object>().str(), fc::ripemd160("1fb1db7e481e4973fb9ea3517d2b379e2a457233").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::market_history::bucket_object>().str(), fc::ripemd160("64f7f320e15f36e252458c47191a2d4057d57d4e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_stats_object>().str(), fc::ripemd160("64b46c1e171ea9e663cccb9a7d83f553c52c02d7").str() );
  #endif
}

BOOST_AUTO_TEST_SUITE_END()

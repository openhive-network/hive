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
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_object>().str(), fc::ripemd160("8dc96b2e035be07f32a78e8e42b58c0f66526a32").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_metadata_object>().str(), fc::ripemd160("a8aa8bca4011ce0de3a11ea456718a046300ed70").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_authority_object>().str(), fc::ripemd160("a77ab4f7199d22feb626968ff72fd21403082e7f").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::vesting_delegation_object>().str(), fc::ripemd160("43b3f7584af10e5a1287069d99ec7e9786448a35").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::vesting_delegation_expiration_object>().str(), fc::ripemd160("656666c3e97e3e4dc23c501fb901d08dc63be743").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::owner_authority_history_object>().str(), fc::ripemd160("ca6953896d79fc117a5f7f67078ec54ea91993bd").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_recovery_request_object>().str(), fc::ripemd160("5807bb232100c28b3b349d7a4fe7e53ef90e99df").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::change_recovery_account_request_object>().str(), fc::ripemd160("1eb2cf5067409307a3a32adc1e364fdd3e8df317").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::block_summary_object>().str(), fc::ripemd160("8c2d8e5d67459703f9ecb187ef2e193a5b8a7742").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_object>().str(), fc::ripemd160("bf9ae2533140d49b1f8e9e087c04e799b422ca2f").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_cashout_ex_object>().str(), fc::ripemd160("a8e35d4ffc5c014d295bf01f7279928155d0b8b7").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_vote_object>().str(), fc::ripemd160("c0766f56299c036c084050e5a139780aea807cdd").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::proposal_object>().str(), fc::ripemd160("b55be9838e2effcbb9e5b819fdaaa3c256a72aea").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::proposal_vote_object>().str(), fc::ripemd160("da3029c4c4356ef3bc7c623b1cf7d9ee456c0db1").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::hardfork_property_object>().str(), fc::ripemd160("7554beca2ff6a1d0f640d4a64b6278afde8573a9").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::convert_request_object>().str(), fc::ripemd160("490a0904addc3ab0490665ea20a790990ec1f738").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::collateralized_convert_request_object>().str(), fc::ripemd160("c0eba4b05b029c233af64071ca6fb9c85ae52c50").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::escrow_object>().str(), fc::ripemd160("c65c8592325aa87651e4119e8c602a622a606e5e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::savings_withdraw_object>().str(), fc::ripemd160("8e05420c36836268541b5c3ea9548d602642c214").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::liquidity_reward_balance_object>().str(), fc::ripemd160("521c4b9ae1e123187ac501269496a8b67163ddf3").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::feed_history_object>().str(), fc::ripemd160("b5214ef50f538d3db4bd95a318b1fb2e47cf0ddd").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::limit_order_object>().str(), fc::ripemd160("b49690814d1f172757738944f90c53f2f5ed12fe").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::withdraw_vesting_route_object>().str(), fc::ripemd160("471f7933cdd03ab6e407003e5a6b5f78b11b734c").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::decline_voting_rights_request_object>().str(), fc::ripemd160("3b0a2986aaff604014f9b3f71b35072a2066be77").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::reward_fund_object>().str(), fc::ripemd160("7af87aa2d2b520d2c4ac15120a25b28679ea5e33").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::recurrent_transfer_object>().str(), fc::ripemd160("c9ef5f1394d949bb3292f581edb2591ead28951d").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::pending_required_action_object>().str(), fc::ripemd160("fd7ef7be02dda386973be590e1a28967c87ad97a").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::pending_optional_action_object>().str(), fc::ripemd160("68d41c4424d0e3cce971e44cb52d53df3797e5e2").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::transaction_object>().str(), fc::ripemd160("1bb49a3a936774df2d410723f5b51fb43527fe6a").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::witness_vote_object>().str(), fc::ripemd160("35c45f88154294f0bb3984b9fb9524f0ddadcdbc").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::witness_schedule_object>().str(), fc::ripemd160("4fb9a89f474354815c70c3da3636e259dcad1a14").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::witness_object>().str(), fc::ripemd160("e5554209f827fc319bf66276c7ff129116cbdcab").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::account_by_key::key_lookup_object>().str(), fc::ripemd160("ca98692875323c26f42dd5996233c921dbda620a").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::account_history_rocksdb::volatile_operation_object>().str(), fc::ripemd160("77083ba98133fc01dada508d46e971fa7b8a79c6").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::block_log_info::block_log_hash_state_object>().str(), fc::ripemd160("2c5c7d8f54cc7399101fdae985197035acc3af30").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::block_log_info::block_log_pending_message_object>().str(), fc::ripemd160("a26837413ff50f98e3c50e9851fe1f14ceec298d").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::follow_object>().str(), fc::ripemd160("045a888ddb322c2a90b207f718e8a9101ba6d52a").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::feed_object>().str(), fc::ripemd160("020b7b02386c0ef5ee104cdb7399d6b3054de6d8").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::blog_object>().str(), fc::ripemd160("0d856f5a46e161fbcc0c5ed776b30bb7faf4cbbb").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::blog_author_stats_object>().str(), fc::ripemd160("15e791950cff9ed2e097c9156289479b9cba0d66").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::reputation_object>().str(), fc::ripemd160("3031082e83950f9499b049ff34f1b2c281b1eed3").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::follow_count_object>().str(), fc::ripemd160("ae5cfac47cebee6b2dc899e2a4ee4b61384e4397").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::market_history::order_history_object>().str(), fc::ripemd160("da0a02afb94eb8da689219c2342e50d590cc7a72").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_resource_param_object>().str(), fc::ripemd160("029f5a4d6c4fdc4c23faceda0d439d9c64f20660").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_pool_object>().str(), fc::ripemd160("0bfc70600695434b140f164db416c03f3274227e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_pending_data>().str(), fc::ripemd160("be80b5603a5b54e96dd51349fbee2f64c29a6a9a").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_account_object>().str(), fc::ripemd160("7d102b1f870e52d98ceebace813faec58c8d4162").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_direct_delegation_object>().str(), fc::ripemd160("6407281ccbeced14a40fad9062f36c6483fc1942").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_usage_bucket_object>().str(), fc::ripemd160("f55323988fcc3be1facb059d311f53cf85532e91").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::reputation::reputation_object>().str(), fc::ripemd160("dd9bbeb34a1da9fb6506912d1095a8e17168d326").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::tag_object>().str(), fc::ripemd160("0a23d5c2a35cfabcfb5a5e7cff73709ddb6a655d").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::tag_stats_object>().str(), fc::ripemd160("2ccba86ccee7fba1bc85d11174c38c66dc6a33de").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::author_tag_stats_object>().str(), fc::ripemd160("cbbbfc8f209e69ab3ee678893ba53eced0508971").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::transaction_status::transaction_status_object>().str(), fc::ripemd160("b11ef4b393b62b7b2105dfbe4ec377378abd7ad3").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::witness::witness_custom_op_object>().str(), fc::ripemd160("dd3f641790e824f4e69b3aaf749273282876f357").str() );

  #ifdef HIVE_ENABLE_SMT
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_token_object>().str(), fc::ripemd160("928cc0f6baf390552a259e7d158d84a8d23607c3").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_regular_balance_object>().str(), fc::ripemd160("24fffd5391ed6a675241619938c5802803892f17").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_rewards_balance_object>().str(), fc::ripemd160("4b21992c9b4b8e6d615b1349ff8aced5d6ac14a5").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::nai_pool_object>().str(), fc::ripemd160("e4b1791e8b252d40ac6d72945b3e6a8fb1f1b0ef").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_token_emissions_object>().str(), fc::ripemd160("3388762a99d7f6d5f22371307ba88abe3b356b43").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_contribution_object>().str(), fc::ripemd160("65fa85737ab01e7ba7efff0e7df6b4ea2d805c39").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_ico_object>().str(), fc::ripemd160("1cf1ccefe12ecf5b2dad2e8d2ce7be915936bff5").str() );

  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_cashout_object>().str(), fc::ripemd160("926d811ad2599156c3f3aa6a9663b13e712a99fe").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::dynamic_global_property_object>().str(), fc::ripemd160("0436720ea24b7a0c99d7bb10fbe0098f6d5d309c").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::market_history::bucket_object>().str(), fc::ripemd160("33e58b076a343ad47a6d3fde820906b28591536b").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_stats_object>().str(), fc::ripemd160("5eb753baa81c43a90d83322be526332fe2325435").str() );
  #else
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_cashout_object>().str(), fc::ripemd160("d9f85722502bfdfb26c9a3bf6ca9b56f8a267022").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::dynamic_global_property_object>().str(), fc::ripemd160("876b17df7563e0ea8d9f2db850005657dc1ff9b6").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::market_history::bucket_object>().str(), fc::ripemd160("d7b0d18d001207ee12b075e582ab32ce90a3084f").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_stats_object>().str(), fc::ripemd160("135022003c73140057075cf80fb4ac38807d4bcc").str() );
  #endif

}

BOOST_AUTO_TEST_SUITE_END()

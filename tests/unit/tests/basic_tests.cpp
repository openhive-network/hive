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
  BOOST_CHECK_EQUAL( alignof( account_object ), 16u );
  BOOST_CHECK_EQUAL( sizeof( account_object ), 464u ); //1.3M+
  BOOST_CHECK_EQUAL( sizeof( account_index::MULTIINDEX_NODE_TYPE ), 656u );
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

BOOST_AUTO_TEST_CASE( decoding_types_mechanism_test )
{
  hive::chain::util::decoded_types_data_storage dtds;

  /* At the beginning we shouldn't have any decoded type data. */
  BOOST_CHECK( dtds.get_decoded_types_data_map().empty() );

  /* Fundamental types - these types are not decoded by decoding mechanism, so decoding types data map should be empty. */
  dtds.register_new_type<int>();
  dtds.register_new_type<float>();
  dtds.register_new_type<double>();
  BOOST_CHECK( dtds.get_decoded_types_data_map().empty() );

  /* Trivial types - like std::less. In this case, we analyse template parameters. In case of fundamental type we again ignore it, but second type should be decoded.*/
  dtds.register_new_type<std::less<int>>();
  dtds.register_new_type<std::less<hive::chain::witness_object::witness_schedule_type>>();
  BOOST_CHECK_EQUAL( dtds.get_decoded_types_data_map().size(), 1 );

  {
    /* hive::chain::witness_object::witness_schedule_type is a reflected enum, so we can get some data about it. */
    const hive::chain::util::decoded_type_data& reflected_enum_data = dtds.get_decoded_type_data<hive::chain::witness_object::witness_schedule_type>();
    BOOST_CHECK( reflected_enum_data.reflected );
    BOOST_CHECK( !reflected_enum_data.members );
    BOOST_CHECK( reflected_enum_data.enum_values );
    BOOST_CHECK_EQUAL( reflected_enum_data.checksum, "8826d3384e563df375fae0b2e00e23d61dee90d8" );
    BOOST_CHECK_EQUAL( reflected_enum_data.type_id, "N4hive5chain14witness_object21witness_schedule_typeE" );
    BOOST_CHECK_EQUAL( *reflected_enum_data.name, "hive::chain::witness_object::witness_schedule_type" );

    const hive::chain::util::decoded_type_data::enum_values_vector_t& enum_values_for_type = *reflected_enum_data.enum_values;
    BOOST_CHECK( !enum_values_for_type.empty() );

    const hive::chain::util::decoded_type_data::enum_values_vector_t specific_enum_values_pattern{{"elected", 0},{"timeshare", 1}, {"miner", 2}, {"none", 3}};

    BOOST_CHECK_EQUAL( enum_values_for_type.size(), specific_enum_values_pattern.size() );

    for (size_t i = 0; i < specific_enum_values_pattern.size(); ++i)
    {
      BOOST_CHECK_EQUAL(enum_values_for_type[i].first, specific_enum_values_pattern[i].first);
      BOOST_CHECK_EQUAL(enum_values_for_type[i].second, specific_enum_values_pattern[i].second);
    }
  }

  /*
    Boost types except index are non reflected which should be added to map. We don't analyse template arguments which are passed to these types.
    In this case, std::string doesn't have decoder (at the time when this comment was written), so if it would be analysed, build error should occur.
    fc::array has a decoder, so if it would be analysed, one more type should be in map.
  */
  dtds.register_new_type<boost::container::basic_string<char>>();
  dtds.register_new_type<boost::container::flat_map<int, std::string>>();
  dtds.register_new_type<boost::container::flat_set<std::string>>();
  dtds.register_new_type<boost::container::deque<char>>();
  dtds.register_new_type<boost::container::vector<fc::array<float, 5>>>();

  BOOST_CHECK_EQUAL( dtds.get_decoded_types_data_map().size(), 6 );

  {
    const hive::chain::util::decoded_type_data& decoded_boost_type = dtds.get_decoded_type_data<boost::container::flat_map<int, std::string>>();

    BOOST_CHECK( !decoded_boost_type.reflected );
    BOOST_CHECK_EQUAL( decoded_boost_type.checksum, "14ac59e65d8cf4341dc8fb7e6d3404a95ab9f5a0");
    BOOST_CHECK_EQUAL( decoded_boost_type.type_id, "N5boost9container8flat_mapIiNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4lessIiEvEE");
    BOOST_CHECK( !decoded_boost_type.name );
    BOOST_CHECK( !decoded_boost_type.members );
    BOOST_CHECK( !decoded_boost_type.enum_values );
  }

  /*
    Decoding an index - boost::multi_index::multi_index_container.
    New non reflected type is added to map + we decode the key type of the index. During this process, at least one more type should be added to map.
    Test case - decode hive::chain::decline_voting_rights_request_index.
    There should be 6 more decoded types: index, decline_voting_rights_request_object, object's oid, fc::time_point_sec,, fc::erpair, fixed_string_impl.
  */

  dtds.register_new_type<hive::chain::decline_voting_rights_request_index>();
  BOOST_CHECK_EQUAL( dtds.get_decoded_types_data_map().size(), 12 );
  {
    const hive::chain::util::decoded_type_data& decoded_type_data = dtds.get_decoded_type_data<hive::chain::decline_voting_rights_request_object>();

    BOOST_CHECK( decoded_type_data.reflected );
    BOOST_CHECK( !decoded_type_data.enum_values );
    BOOST_CHECK( decoded_type_data.members );
    BOOST_CHECK_EQUAL( decoded_type_data.checksum, "07a72f6d76870fbab70cd7bbfb8e5b5e92a2bcbc" );
    BOOST_CHECK_EQUAL( decoded_type_data.type_id, "N4hive5chain36decline_voting_rights_request_objectE" );
    BOOST_CHECK_EQUAL( *decoded_type_data.name, "hive::chain::decline_voting_rights_request_object" );

    const hive::chain::util::decoded_type_data::members_vector_t& type_members = *decoded_type_data.members;
    BOOST_CHECK( !type_members.empty() );

    const hive::chain::util::decoded_type_data::members_vector_t type_members_pattern
      {{.type = "chainbase::oid<hive::chain::decline_voting_rights_request_object>", .name = "id", .offset = 0},
       {.type = "hive::protocol::account_name_type", .name = "account", .offset = 8},
       {.type = "fc::time_point_sec", .name = "effective_date", .offset = 24}};

    BOOST_CHECK_EQUAL( type_members.size(), type_members_pattern.size() );

    for (size_t i = 0; i < type_members_pattern.size(); ++i)
    {
      BOOST_CHECK_EQUAL(type_members[i].type, type_members_pattern[i].type);
      BOOST_CHECK_EQUAL(type_members[i].name, type_members_pattern[i].name);
      BOOST_CHECK_EQUAL(type_members[i].offset, type_members_pattern[i].offset);
    }

    const hive::chain::util::decoded_type_data& decoded_index_data = dtds.get_decoded_type_data<hive::chain::decline_voting_rights_request_index>();
    BOOST_CHECK( !decoded_index_data.reflected );

    const hive::chain::util::decoded_type_data& decoded_type_data_tps = dtds.get_decoded_type_data<fc::time_point_sec>();
    BOOST_CHECK( !decoded_type_data_tps.reflected );

    const hive::chain::util::decoded_type_data& decoded_type_data_fcerpair = dtds.get_decoded_type_data<fc::erpair< uint64_t, uint64_t >>();
    BOOST_CHECK( !decoded_type_data_fcerpair.reflected );

    BOOST_CHECK_EQUAL( dtds.get_decoded_types_data_map().size(), 12 ); // there should be no new types in map, because object is already decoded because index was decoded.
  }

  /* Decoding type for which specific decored is defined. For fc::ripemd160, only one decoded type data should be added to map. */
  dtds.register_new_type<fc::ripemd160>();
  BOOST_CHECK_EQUAL( dtds.get_decoded_types_data_map().size(), 13 );
  {
    const hive::chain::util::decoded_type_data& decoded_non_reflected_type = dtds.get_decoded_type_data<fc::ripemd160>();
    BOOST_CHECK( !decoded_non_reflected_type.reflected );
    BOOST_CHECK( !decoded_non_reflected_type.name );
    BOOST_CHECK( !decoded_non_reflected_type.members );
    BOOST_CHECK( !decoded_non_reflected_type.enum_values );
    BOOST_CHECK_EQUAL( decoded_non_reflected_type.checksum, "686344869324c953019f65bef15816ca48c831d4" );
    BOOST_CHECK_EQUAL( decoded_non_reflected_type.type_id, "N2fc9ripemd160E" );
  }

  /*
    Decoding reflected type - account_object. This type contains other reflected and non reflected object.
    We should have new types like: hive::protocol::public_key_type (reflected) and hive::chain::account_object (reflected).
  */
  dtds.register_new_type<hive::chain::account_object>();
  BOOST_CHECK_EQUAL( dtds.get_decoded_types_data_map().size(), 28 );
  {
    const hive::chain::util::decoded_type_data& decoded_public_key_type = dtds.get_decoded_type_data<hive::protocol::public_key_type>();

    BOOST_CHECK( decoded_public_key_type.reflected );
    BOOST_CHECK( !decoded_public_key_type.enum_values );
    BOOST_CHECK( decoded_public_key_type.members );
    BOOST_CHECK_EQUAL( decoded_public_key_type.checksum, "8acb3034188086a3630d70c44b1ddb21fe0f8d1f" );
    BOOST_CHECK_EQUAL( decoded_public_key_type.type_id, "N4hive8protocol15public_key_typeE" );
    BOOST_CHECK_EQUAL( *decoded_public_key_type.name, "hive::protocol::public_key_type" );

    const hive::chain::util::decoded_type_data::members_vector_t& public_key_members = *decoded_public_key_type.members;

    BOOST_CHECK( !public_key_members.empty() );

    const hive::chain::util::decoded_type_data::members_vector_t public_key_members_pattern {{.type = "fc::array<char,33>", .name = "key_data", .offset = 0}};
    BOOST_CHECK_EQUAL( public_key_members.size(), public_key_members_pattern.size() );

    for (size_t i = 0; i < public_key_members_pattern.size(); ++i)
    {
      BOOST_CHECK_EQUAL(public_key_members[i].type, public_key_members_pattern[i].type);
      BOOST_CHECK_EQUAL(public_key_members[i].name, public_key_members_pattern[i].name);
      BOOST_CHECK_EQUAL(public_key_members[i].offset, public_key_members_pattern[i].offset);
    }

    const hive::chain::util::decoded_type_data& decoded_fc_array = dtds.get_decoded_type_data<fc::array<char,33>>();
    BOOST_CHECK( !decoded_fc_array.reflected );

    const hive::chain::util::decoded_type_data& decoded_account_object = dtds.get_decoded_type_data<hive::chain::account_object>();
    BOOST_CHECK( decoded_account_object.reflected );
    BOOST_CHECK( !decoded_account_object.enum_values );
    BOOST_CHECK( decoded_account_object.members );
    BOOST_CHECK_EQUAL( decoded_account_object.members->size(), 56 );
  }

  BOOST_CHECK_EQUAL( dtds.get_decoded_types_data_map().size(), 28 ); // decoded types map size shouldn't change.

  BOOST_CHECK_NO_THROW(dtds.register_new_type<fc::static_variant<>>());
  BOOST_CHECK_NO_THROW(dtds.register_new_type<fc::static_variant<int>>());
  BOOST_CHECK_EQUAL( dtds.get_decoded_types_data_map().size(), 30 );
}

BOOST_AUTO_TEST_CASE( decoded_type_data_json_operations )
{
  hive::chain::util::decoded_types_data_storage dtds;
  {
    // 1. reflected enum.
    dtds.register_new_type<hive::chain::witness_object::witness_schedule_type>();
    const hive::chain::util::decoded_type_data& decoded_witness_schedule_type = dtds.get_decoded_type_data<hive::chain::witness_object::witness_schedule_type>();
    const std::string decoded_witness_schedule_type_str = fc::json::to_string(decoded_witness_schedule_type);
    hive::chain::util::decoded_type_data created_decoded_witness_schedule_type(decoded_witness_schedule_type_str);

    BOOST_CHECK_EQUAL(created_decoded_witness_schedule_type.checksum, decoded_witness_schedule_type.checksum);
    BOOST_CHECK_EQUAL(created_decoded_witness_schedule_type.type_id, decoded_witness_schedule_type.type_id);
    BOOST_CHECK_EQUAL(*created_decoded_witness_schedule_type.name, *decoded_witness_schedule_type.name);
    BOOST_CHECK_EQUAL(created_decoded_witness_schedule_type.reflected, decoded_witness_schedule_type.reflected);
    BOOST_CHECK(created_decoded_witness_schedule_type.enum_values);
    BOOST_CHECK(decoded_witness_schedule_type.enum_values);
    BOOST_CHECK(!created_decoded_witness_schedule_type.members);
    BOOST_CHECK(!decoded_witness_schedule_type.members);
    BOOST_CHECK_EQUAL(created_decoded_witness_schedule_type.enum_values->size(), decoded_witness_schedule_type.enum_values->size());
    BOOST_CHECK_EQUAL(fc::json::to_string(created_decoded_witness_schedule_type), decoded_witness_schedule_type_str);
  }
  {
    // 2. reflected type.
    dtds.register_new_type<hive::chain::decline_voting_rights_request_object>();
    const hive::chain::util::decoded_type_data& decoded_decline_voting_rights_request_object = dtds.get_decoded_type_data<hive::chain::decline_voting_rights_request_object>();
    const std::string decoded_decline_voting_rights_request_object_str = fc::json::to_string(decoded_decline_voting_rights_request_object);
    hive::chain::util::decoded_type_data created_decoded_decline_voting_rights_request_object(decoded_decline_voting_rights_request_object_str);

    BOOST_CHECK_EQUAL(created_decoded_decline_voting_rights_request_object.checksum, decoded_decline_voting_rights_request_object.checksum);
    BOOST_CHECK_EQUAL(created_decoded_decline_voting_rights_request_object.type_id, decoded_decline_voting_rights_request_object.type_id);
    BOOST_CHECK_EQUAL(*created_decoded_decline_voting_rights_request_object.name, *decoded_decline_voting_rights_request_object.name);
    BOOST_CHECK_EQUAL(created_decoded_decline_voting_rights_request_object.reflected, decoded_decline_voting_rights_request_object.reflected);
    BOOST_CHECK(!created_decoded_decline_voting_rights_request_object.enum_values);
    BOOST_CHECK(!decoded_decline_voting_rights_request_object.enum_values);
    BOOST_CHECK(created_decoded_decline_voting_rights_request_object.members);
    BOOST_CHECK(decoded_decline_voting_rights_request_object.members);
    BOOST_CHECK_EQUAL(created_decoded_decline_voting_rights_request_object.members->size(), decoded_decline_voting_rights_request_object.members->size());
    BOOST_CHECK_EQUAL(fc::json::to_string(created_decoded_decline_voting_rights_request_object), decoded_decline_voting_rights_request_object_str);
  }
  {
    // 3. Non reflected type.
    dtds.register_new_type<fc::time_point_sec>();
    const hive::chain::util::decoded_type_data& decoded_time_point_sec = dtds.get_decoded_type_data<fc::time_point_sec>();
    const std::string decoded_time_point_sec_str = fc::json::to_string(decoded_time_point_sec);
    hive::chain::util::decoded_type_data created_decoded_time_point_sec(decoded_time_point_sec_str);

    BOOST_CHECK_EQUAL(created_decoded_time_point_sec.checksum, decoded_time_point_sec.checksum);
    BOOST_CHECK_EQUAL(created_decoded_time_point_sec.type_id, decoded_time_point_sec.type_id);
    BOOST_CHECK_EQUAL(created_decoded_time_point_sec.reflected, decoded_time_point_sec.reflected);
    BOOST_CHECK(!created_decoded_time_point_sec.members);
    BOOST_CHECK(!created_decoded_time_point_sec.enum_values);
    BOOST_CHECK(!created_decoded_time_point_sec.name);
    BOOST_CHECK_EQUAL(fc::json::to_string(created_decoded_time_point_sec), decoded_time_point_sec_str);
  }
  {
    // 4. Error while creating  reflected or nonreflected decoded type.
   BOOST_CHECK_THROW(hive::chain::util::decoded_type_data(""), fc::invalid_arg_exception);
   {
    fc::variant_object_builder builder;
    builder("type_id","asdf")("checksum","zxcv");
    BOOST_CHECK_THROW(hive::chain::util::decoded_type_data(fc::json::to_string(builder.get())), fc::invalid_arg_exception);
   }
   {
    fc::variant_object_builder builder;
    builder("reflected",false)("type_id","zxcv");
    BOOST_CHECK_THROW(hive::chain::util::decoded_type_data(fc::json::to_string(builder.get())), fc::invalid_arg_exception);
   }
   {
    fc::variant_object_builder builder;
    builder("reflected",false)("checksum","asdf");
    BOOST_CHECK_THROW(hive::chain::util::decoded_type_data(fc::json::to_string(builder.get())), fc::invalid_arg_exception);
   }
   {
    fc::variant_object_builder builder;
    builder("reflected","zxcv")("type_id","zxcv")("checksum","asdf");
    BOOST_CHECK_THROW(hive::chain::util::decoded_type_data(fc::json::to_string(builder.get())), fc::bad_cast_exception);
   }
   {
    fc::variant_object_builder builder;
    builder("reflected",true)("type_id","zxcv")("checksum","asdf")("name","zxcv");
    BOOST_CHECK_THROW(hive::chain::util::decoded_type_data(fc::json::to_string(builder.get())), fc::invalid_arg_exception);
   }
   {
    fc::variant_object_builder builder;
    builder("reflected",true)("type_id","zxcv")("checksum","asdf")("name","zxcv")("enum_values", fc::variant_object());
    BOOST_CHECK_THROW(hive::chain::util::decoded_type_data(fc::json::to_string(builder.get())), fc::bad_cast_exception);
   }
   {
    fc::variant_object_builder builder;
    builder("reflected",true)("type_id","zxcv")("checksum","asdf")("name","zxcv")("members", fc::variant_object());
    BOOST_CHECK_THROW(hive::chain::util::decoded_type_data(fc::json::to_string(builder.get())), fc::bad_cast_exception);
   }
   {
    fc::variant_object_builder builder;
    hive::chain::util::decoded_type_data::members_vector_t members_patters{{.type="zxcv",.name="asdf",.offset=0}};
    builder("reflected",false)("type_id","zxcv")("checksum","asdf")("name","zxcv")("members", members_patters);
    BOOST_CHECK_THROW(hive::chain::util::decoded_type_data(fc::json::to_string(builder.get())), fc::invalid_arg_exception);
   }
  }
  {
    // 5. Generate json by decoded_types_data_storage. Comparing method should return true - data generated by json is the same data which is kept in map.
    const std::string current_decoded_types_data_in_json = dtds.generate_decoded_types_data_json_string();
    const std::string json_pattern = "["
      "{\"name\":\"hive::chain::decline_voting_rights_request_object\",\"members\":["
        "{\"type\":\"chainbase::oid<hive::chain::decline_voting_rights_request_object>\",\"name\":\"id\",\"offset\":0},"
        "{\"type\":\"hive::protocol::account_name_type\",\"name\":\"account\",\"offset\":8},"
        "{\"type\":\"fc::time_point_sec\",\"name\":\"effective_date\",\"offset\":24}],"
      "\"type_id\":\"N4hive5chain36decline_voting_rights_request_objectE\",\"checksum\":\"07a72f6d76870fbab70cd7bbfb8e5b5e92a2bcbc\",\"reflected\":true},"
      "{\"type_id\":\"N2fc14time_point_secE\",\"checksum\":\"caf66a3ac4e50b6f285ada8954de203a54ac98cf\",\"reflected\":false},"
      "{\"type_id\":\"N2fc6erpairImmEE\",\"checksum\":\"00b25a6ab8226db3a31e2582dc70e0b8f508ba21\",\"reflected\":false},"
      "{\"type_id\":\"N4hive8protocol17fixed_string_implIN2fc6erpairImmEEEE\",\"checksum\":\"e7a2f4780f492fef01378693217d9e7358757dcd\",\"reflected\":false},"
      "{\"type_id\":\"N9chainbase3oidIN4hive5chain36decline_voting_rights_request_objectEEE\",\"checksum\":\"cd1883f4c4665b69da64199ce965393405513f21\",\"reflected\":false},"
      "{\"name\":\"hive::chain::witness_object::witness_schedule_type\",\"enum_values\":["
      "[\"elected\",0],[\"timeshare\",1],[\"miner\",2],[\"none\",3]],"
      "\"type_id\":\"N4hive5chain14witness_object21witness_schedule_typeE\",\"checksum\":\"8826d3384e563df375fae0b2e00e23d61dee90d8\",\"reflected\":true}"
      "]";

    BOOST_CHECK_EQUAL(current_decoded_types_data_in_json, json_pattern);
    const auto response = dtds.check_if_decoded_types_data_json_matches_with_current_decoded_data(current_decoded_types_data_in_json);
    BOOST_CHECK(response.first);
    BOOST_CHECK(response.second.empty());
  }
  {
    // 6. passing json with should not match to current decoded types map data.
    const std::string wrong_json_pattern = "["
      "{\"reflected\":true,\"type_id\":\"N4hive5chain14witness_object21witness_schedule_typeE\",\"checksum\":\"8826d3384e563df375fae0b2e00e23d61dee90d5\",\"name\":\"hive::chain::witness_object::witness_schedule_type\","
        "\"enum_values\":[[\"elected\",0],[\"timeshare\",1],[\"miner\",2],[\"none\",3]]}"
      "{\"reflected\":false,\"type_id\":\"N9chainbase3oidIN4hive5chain29withdraw_vesting_route_objectEEE\",\"checksum\":\"9f9a4d9defc72dbc8bdcdbba04e02e4bd136c3b6\"}"
      "]";

    const auto response = dtds.check_if_decoded_types_data_json_matches_with_current_decoded_data(wrong_json_pattern);
    BOOST_CHECK(!response.first);

    const std::string response_pattern =
    "Amount of decoded types differs from amount of loaded decoded types. Current amount of decoded types: 6, loaded amount of decoded types: 2\n"
    "Type is in current decoded types map but not in loaded decoded types map: hive::chain::decline_voting_rights_request_object\n"
    "Type is in current decoded types map but not in loaded decoded types map: N2fc14time_point_secE\n"
    "Type is in current decoded types map but not in loaded decoded types map: N2fc6erpairImmEE\n"
    "Type is in current decoded types map but not in loaded decoded types map: N4hive8protocol17fixed_string_implIN2fc6erpairImmEEEE\n"
    "Type is in current decoded types map but not in loaded decoded types map: N9chainbase3oidIN4hive5chain36decline_voting_rights_request_objectEEE\n"
    "Reflected type: hive::chain::witness_object::witness_schedule_type has checksum: 8826d3384e563df375fae0b2e00e23d61dee90d8, which diffs from loaded type: 8826d3384e563df375fae0b2e00e23d61dee90d5\n"
    "Type is in loaded decoded types map but not in current decoded types map: N9chainbase3oidIN4hive5chain29withdraw_vesting_route_objectEEE\n";

    BOOST_CHECK_EQUAL(response.second, response_pattern);
  }
  {
    // 7. Pass wrong json or something else than json. Exception should be thrown.
    BOOST_CHECK_THROW(dtds.check_if_decoded_types_data_json_matches_with_current_decoded_data("asdf"), fc::parse_error_exception);
    BOOST_CHECK_THROW(dtds.check_if_decoded_types_data_json_matches_with_current_decoded_data(""), fc::invalid_arg_exception);
  }
}

template <typename T>
std::string_view get_decoded_type_checksum(hive::chain::util::decoded_types_data_storage& dtds)
{
  dtds.register_new_type<T>();
  return dtds.get_decoded_type_checksum<T>();
}

BOOST_AUTO_TEST_CASE( chain_object_checksum )
{
  hive::chain::util::decoded_types_data_storage dtds;

  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::account_object>(dtds), "f123e4e507bf0792ecf3c974819bb6850e967708" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::account_metadata_object>(dtds), "8afa67ed15888d3f927bcb2b986d172da1f34375" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::account_authority_object>(dtds), "918c06ba905b0b095aa62df1b432e823790df6ba" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::vesting_delegation_object>(dtds), "7a6cd5d88139e0c501e52ad5c26b2def2efd07a0" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::vesting_delegation_expiration_object>(dtds), "6194b9b2563550582ddb573fe52b1e21a4437e3b" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::owner_authority_history_object>(dtds), "9c5112ba809aecc4f87540ac24409b292fa35fa7" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::account_recovery_request_object>(dtds), "cd4d092d7ad8808bdb495fbe7c4f3b5720684ec3" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::change_recovery_account_request_object>(dtds), "38d9aca36a34fd424be5053c89f989f727ecb5f2" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::block_summary_object>(dtds), "5894437d8fbfc5b21a6dd12a6a49c88c5be4b9b7" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::comment_object>(dtds), "4675213b6f61c5050a1669f3202048ea6752169f" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::comment_cashout_ex_object>(dtds), "5bd998eef24ebee5fe5bf1a935956520ad44f0f2" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::comment_vote_object>(dtds), "8ebf568ac759d00ee1d53b456e52edaa3791b56e" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::proposal_object>(dtds), "bfe417071b06c7ea78a1aa42dc54a87e39bced73" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::proposal_vote_object>(dtds), "045fe6cfecd0e66286a513210a177f0266a2fd69" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::hardfork_property_object>(dtds), "eccc5f45ebfe5c8f74e5ea16d1251ff9a8d09d0d" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::convert_request_object>(dtds), "152458a15c66285cd4b040847eb40edbb88a1400" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::collateralized_convert_request_object>(dtds), "4f7ed0cb6ec3349e3b910c907e49ea70820de399" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::escrow_object>(dtds), "fdb1ce7097af1cb3af336ff98a4b344131f581be" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::savings_withdraw_object>(dtds), "2bac6b38b5ffcd66d3222cb76c16f0660b178000" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::liquidity_reward_balance_object>(dtds), "51a39bcd4ac2f6848e5f3c73d36fccbe8bbf5cc0" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::feed_history_object>(dtds), "0920ae5e3416aa63d93bd183d8b0097a6042c9e0" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::limit_order_object>(dtds), "c9ecc3d754aa3d6b118ebcc5719dac2089c04c0f" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::withdraw_vesting_route_object>(dtds), "de94693e25f3f1182c39e77cfaff328098b88ba8" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::decline_voting_rights_request_object>(dtds), "07a72f6d76870fbab70cd7bbfb8e5b5e92a2bcbc" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::reward_fund_object>(dtds), "377f79d55fea6d0828d8ac7f22ec6c453b393d2a" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::recurrent_transfer_object>(dtds), "e2ac0aa6c2023129d32b5ab81db2d25f5a6c0ec3" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::pending_required_action_object>(dtds), "3d017c3490841bd5b11c552eac030830d8e1642e" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::pending_optional_action_object>(dtds), "bde29654096d4db8cb4397896685131e50221f5c" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::transaction_object>(dtds), "34c3f519ba27dedbf0bf95657acd753bb62e469c" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::witness_vote_object>(dtds), "7fba3dd9aa976deb4b9233ed2a7ad29204c7f991" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::witness_schedule_object>(dtds), "f942aae63947afa6ff5e60510b49926c72be6fef" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::witness_object>(dtds), "03e1da5feb5e8d88d4a61449a638f6ec0f63b504" );

  #ifdef HIVE_ENABLE_SMT
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::smt_token_object>(dtds), "d0560917541f173f8aa06909e8f718a2d502b39d" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::account_regular_balance_object>(dtds), "29676138808502f7a8f16f1df815143dab536814" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::account_rewards_balance_object>(dtds), "ec01d2d1a2f207fb68430d645fe4d43f24d578f7" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::nai_pool_object>(dtds), "de4aac8e6b7f52a1f08e6d8daacdb46275a2bc02" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::smt_token_emissions_object>(dtds), "5cb6a0a11dc7ca3f85a37105502374ad1dfab806" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::smt_contribution_object>(dtds), "53a5133aed8fe290feccc31f88c1251c6841d465" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::smt_ico_object>(dtds), "64caa9ed82fef9b70b246c1e8fd36963a457942b" );

  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::comment_cashout_object>(dtds), "53daa2994a6ed66b6ba3426b9640a5f372c5d2fa" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::dynamic_global_property_object>(dtds), "d4e768d340b4704e1948a374449ce984c01d5789" );
  #else
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::comment_cashout_object>(dtds), "852b9160388cf2fd59c105d4a27e016206281ef9" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::chain::dynamic_global_property_object>(dtds), "7d0cd80256810267856bc72e273a360352a00c9b" );
  #endif

}

BOOST_AUTO_TEST_SUITE_END()

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

#include <fc/crypto/digest.hpp>
#include <fc/crypto/hex.hpp>
#include "../db_fixture/database_fixture.hpp"

#include <algorithm>
#include <random>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;

BOOST_FIXTURE_TEST_SUITE( basic_tests, clean_database_fixture )

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

uint8_t find_msb( const uint128_t& u )
{
  uint64_t x;
  uint8_t places;
  x      = (u.lo ? u.lo : 1);
  places = (u.hi ?   64 : 0);
  x      = (u.hi ? u.hi : x);
  return uint8_t( boost::multiprecision::detail::find_msb(x) + places );
}

uint64_t approx_sqrt( const uint128_t& x )
{
  if( (x.lo == 0) && (x.hi == 0) )
    return 0;

  uint8_t msb_x = find_msb(x);
  uint8_t msb_z = msb_x >> 1;

  uint128_t msb_x_bit = uint128_t(1) << msb_x;
  uint64_t  msb_z_bit = uint64_t (1) << msb_z;

  uint128_t mantissa_mask = msb_x_bit - 1;
  uint128_t mantissa_x = x & mantissa_mask;
  uint64_t mantissa_z_hi = (msb_x & 1) ? msb_z_bit : 0;
  uint64_t mantissa_z_lo = (mantissa_x >> (msb_x - msb_z)).lo;
  uint64_t mantissa_z = (mantissa_z_hi | mantissa_z_lo) >> 1;
  uint64_t result = msb_z_bit | mantissa_z;

  return result;
}

BOOST_AUTO_TEST_CASE( curation_weight_test )
{
  fc::uint128_t rshares = 856158;
  fc::uint128_t s = uint128_t( 0, 2000000000000ull );
  fc::uint128_t sqrt = approx_sqrt( rshares + 2 * s );
  uint64_t result = ( rshares / sqrt ).to_uint64();

  BOOST_REQUIRE( sqrt.to_uint64() == 2002250 );
  BOOST_REQUIRE( result == 0 );

  rshares = 0;
  sqrt = approx_sqrt( rshares + 2 * s );
  result = ( rshares / sqrt ).to_uint64();

  BOOST_REQUIRE( sqrt.to_uint64() == 2002250 );
  BOOST_REQUIRE( result == 0 );

  result = ( uint128_t( 0 ) - uint128_t( 0 ) ).to_uint64();

  BOOST_REQUIRE( result == 0 );
  rshares = uint128_t( 0, 3351842535167ull );

  for( int64_t i = 856158; i >= 0; --i )
  {
    uint64_t old_weight = util::evaluate_reward_curve( rshares - i, protocol::convergent_square_root, s ).to_uint64();
    uint64_t new_weight = util::evaluate_reward_curve( rshares, protocol::convergent_square_root, s ).to_uint64();

    BOOST_REQUIRE( old_weight <= new_weight );

    uint128_t w( new_weight - old_weight );

    w *= 300;
    w /= 300;
    BOOST_REQUIRE( w.to_uint64() == new_weight - old_weight );
  }

  //idump( (delta)(old_weight)(new_weight) );

}

#ifndef ENABLE_STD_ALLOCATOR
BOOST_AUTO_TEST_CASE( chain_object_size )
{
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
  BOOST_CHECK_EQUAL( sizeof( account_object ), 424u ); //1.3M+
  BOOST_CHECK_EQUAL( sizeof( account_index::MULTIINDEX_NODE_TYPE ), 616u );
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
  BOOST_CHECK_EQUAL( sizeof( reward_fund_object ), 96u );
  BOOST_CHECK_EQUAL( sizeof( reward_fund_index::MULTIINDEX_NODE_TYPE ), 160u );
  BOOST_CHECK_EQUAL( sizeof( dynamic_global_property_object ), 368u
#ifdef HIVE_ENABLE_SMT
    + 16
#endif
  );
  BOOST_CHECK_EQUAL( sizeof( dynamic_global_property_index::MULTIINDEX_NODE_TYPE ), 400u
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
  BOOST_CHECK_EQUAL( sizeof( witness_schedule_object ), 536u );
  BOOST_CHECK_EQUAL( sizeof( witness_schedule_index::MULTIINDEX_NODE_TYPE ), 568u );

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

  BOOST_CHECK_EQUAL( sizeof( full_transaction_type ), 488 ); //not a chain object but potentially very numerous
}
#endif

BOOST_AUTO_TEST_SUITE_END()

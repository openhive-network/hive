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
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_object>().str(), fc::ripemd160("fe58ef9608c5f2223789f047810142b1075cd140").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_metadata_object>().str(), fc::ripemd160("5fafcf70b1e68dc97fb22ab76f15bfe89bac3019").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_authority_object>().str(), fc::ripemd160("01f82a5e4808942880d7ecae9fc84510a197c216").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::vesting_delegation_object>().str(), fc::ripemd160("448cb5791f967ec4732e5d6bde5e8e850509f90b").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::vesting_delegation_expiration_object>().str(), fc::ripemd160("410155a0f6c819ba1ee457d98e3d542bc64dc075").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::owner_authority_history_object>().str(), fc::ripemd160("cfc42d7c230df0858328b01d53f9675b814cf83d").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_recovery_request_object>().str(), fc::ripemd160("727b810a94669c6e630b4439d20dc2129df7a3de").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::change_recovery_account_request_object>().str(), fc::ripemd160("22c9405ba8436969d152e840c8eefe7f59203549").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::block_summary_object>().str(), fc::ripemd160("4e0432e9fa904f38d867d95c9c2aea073c3d1bc2").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_object>().str(), fc::ripemd160("a74723fc5df010e5a593af301b9f98fb35260efe").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_cashout_ex_object>().str(), fc::ripemd160("689e625553b63138ec80a7898b44455c8a22ca76").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_vote_object>().str(), fc::ripemd160("f05c12757356cd412fd85b2f4f4f60c35e478671").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::proposal_object>().str(), fc::ripemd160("906f89810ad02efc18eb99fa49cfedbb080f6e5e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::proposal_vote_object>().str(), fc::ripemd160("bb953c8b5e88a9a3891acc7831e702e1a9e328e3").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::hardfork_property_object>().str(), fc::ripemd160("dc3a8ecd780c1f0e5bff451c2c25b9552008a74c").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::convert_request_object>().str(), fc::ripemd160("ff61ae13702a2f749381f47ea3027bd90cb48709").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::collateralized_convert_request_object>().str(), fc::ripemd160("0c37e409b2941364af5731ed0e27cc0701b5022f").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::escrow_object>().str(), fc::ripemd160("f65c8e86d49194b54439023f04de7deb29614510").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::savings_withdraw_object>().str(), fc::ripemd160("09451f99746270af01a6fca9340e7e8a3317de66").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::liquidity_reward_balance_object>().str(), fc::ripemd160("d88eaf2bef611d5d0c2c4c4943b67837d30b0aba").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::feed_history_object>().str(), fc::ripemd160("9e13735a53a819c54edbb6a4d2f374337a3d6087").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::limit_order_object>().str(), fc::ripemd160("80868a62d427406ba61c157dc6ce0c867fba0560").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::withdraw_vesting_route_object>().str(), fc::ripemd160("f1b30e7ec3e008e90b4e0f7899c97a26630c39d2").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::decline_voting_rights_request_object>().str(), fc::ripemd160("48e8ee6586baa60530494eb107f30fc8727a0dcb").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::reward_fund_object>().str(), fc::ripemd160("f3691856f31af7d65db59b07b8662360a279665d").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::recurrent_transfer_object>().str(), fc::ripemd160("a7db8a854a12520f5567cc240e439a01f0e3013f").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::pending_required_action_object>().str(), fc::ripemd160("73f418063418c2df1fde8c77e71abf83fb7dbcc2").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::pending_optional_action_object>().str(), fc::ripemd160("0de7464820d10d8641e408d57ea8ffadb749024a").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::transaction_object>().str(), fc::ripemd160("5be393a826c0465ca3961010194117fa2d57f2de").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::witness_vote_object>().str(), fc::ripemd160("1d27a19f7b958fd59f005ce80c3706b33f8d2dce").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::witness_schedule_object>().str(), fc::ripemd160("fef3ca62f91bbc0e839d1cf196da63464d07d67b").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::witness_object>().str(), fc::ripemd160("ecc409d58be0102203aef818733a3d4539415a39").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::account_by_key::key_lookup_object>().str(), fc::ripemd160("2698ff8bfb8d86d8fd87b4a6a1420ef71e445683").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::account_history_rocksdb::volatile_operation_object>().str(), fc::ripemd160("0901b2b7424aabd8563ca29f6ff0e1082801c614").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::block_log_info::block_log_hash_state_object>().str(), fc::ripemd160("10c39a161047d6b10d119e90f5e672f716397d4f").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::block_log_info::block_log_pending_message_object>().str(), fc::ripemd160("f2a88578ca6baeea33876f6a9ca0592bedbc8078").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::follow_object>().str(), fc::ripemd160("6261879574a87443567e453ef058031dcd1ee8a5").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::feed_object>().str(), fc::ripemd160("d4bdb9cbec08f8caff43c0939dc6fbca917d7edd").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::blog_object>().str(), fc::ripemd160("de6cbb251d8bd71cc769340f1591b39d2a873cc4").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::blog_author_stats_object>().str(), fc::ripemd160("e9224c238ce549413b6137fa4a5fcc22880bf069").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::reputation_object>().str(), fc::ripemd160("37be7cce44764119421b7b0f4e831cc44e25ad65").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::follow::follow_count_object>().str(), fc::ripemd160("f4a2bd7f61b1beeb050b3a7463f69d83e23ed06a").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::market_history::order_history_object>().str(), fc::ripemd160("2c9484cec5f7b0d18f4aed228713ad91c4a3c85c").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_resource_param_object>().str(), fc::ripemd160("a4726cedceea76e214de61c2595f5cdb817660b3").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_pool_object>().str(), fc::ripemd160("352fb24f8749dc9d1888db6b7eb0f7aa49684fa4").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_pending_data>().str(), fc::ripemd160("8a243a75f335ae9095f56e6378c8031c15904465").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_account_object>().str(), fc::ripemd160("bc7600aba1b0501f4e80eb52c1f64fb93db550c6").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_direct_delegation_object>().str(), fc::ripemd160("16b457adb9df57a7c94ca3ec04e56ec82f1d4d58").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_usage_bucket_object>().str(), fc::ripemd160("d19a0b9a7f2fd6785c70ed9970630e9c2a2c070b").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::reputation::reputation_object>().str(), fc::ripemd160("07c3a924ebd476d070a8ace43e34ffcbaebd9aae").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::tag_object>().str(), fc::ripemd160("fef7f7f339a38d64a22b6910322d59a7634a8677").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::tag_stats_object>().str(), fc::ripemd160("2d13ef1dbde0694f6786a4d1e8e3cc8b3ff13eea").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::author_tag_stats_object>().str(), fc::ripemd160("56a6762af870714986ecc2e02ab38909dead31ed").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::transaction_status::transaction_status_object>().str(), fc::ripemd160("7fad96fe9a2d75134f8db31febeb17add642cd1c").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::witness::witness_custom_op_object>().str(), fc::ripemd160("180b4c43a9ec98ee76cb432a91c0e7f07b006e62").str() );

  #ifdef HIVE_ENABLE_SMT
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_token_object>().str(), fc::ripemd160("72aa1d8c9b79fc7078830f7c9d90b073310414da").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_regular_balance_object>().str(), fc::ripemd160("aab3c411369077ff063e320560eaf05f12027f12").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::account_rewards_balance_object>().str(), fc::ripemd160("7995dea1c5e7c7622acefa9db517aa27b316437b").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::nai_pool_object>().str(), fc::ripemd160("a32c332ff4ea0b1eb63577d76e862617ddd61a89").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_token_emissions_object>().str(), fc::ripemd160("a56ac95b34a86e12eb2eb920bfe79182bc6f645c").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_contribution_object>().str(), fc::ripemd160("fe0b1ae85a9506f34f0b6288c4c577d1e8e3b7bd").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::smt_ico_object>().str(), fc::ripemd160("050309363410ef8de65a1e712f946f4fa46d74e3").str() );

  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_cashout_object>().str(), fc::ripemd160("ba8023ae23e04b5a63a47e74d2d870668df6ec45").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::dynamic_global_property_object>().str(), fc::ripemd160("8d507ee57914b77abe0fbecdf9ac0f3790889b05").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::market_history::bucket_object>().str(), fc::ripemd160("37d2d23cb398931ab2d93da2f06b03855de15f8c").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_stats_object>().str(), fc::ripemd160("19f393e2040ac02d298868d02c2626faee7d495f").str() );
  #else
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::comment_cashout_object>().str(), fc::ripemd160("8134c67bc8f799069862bd1fa7b51a0f1260f533").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::chain::dynamic_global_property_object>().str(), fc::ripemd160("57a3bb79cb96fe8fe7296ab1152bb9ea5b2d47e7").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::market_history::bucket_object>().str(), fc::ripemd160("c08898ad07b74aabd5638567f0acb4cf916dbeb5").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_stats_object>().str(), fc::ripemd160("8aa71acc0861141f7c5f5e90461036ec9a2d03c5").str() );
  #endif
}

BOOST_AUTO_TEST_SUITE_END()

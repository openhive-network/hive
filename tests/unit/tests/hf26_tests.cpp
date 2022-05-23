#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/util/owner_update_limit_mgr.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::chain::util;

BOOST_FIXTURE_TEST_SUITE( hf26_tests, cluster_database_fixture )

BOOST_AUTO_TEST_CASE( update_operation )
{
  try
  {
    struct data
    {
      std::string old_key;
      std::string new_key;

      bool is_exception = false;
    };

    std::vector<data> _v
    { 
      { "bob_owner",    "key number 01", false },
      { "key number 01", "key number 02", true },
      { "key number 01", "key number 03", true }
    };

    std::vector<data> _v_hf26 =
    { 
      { "bob_owner",    "key number 01", false },
      { "key number 01", "key number 02", false },
      { "key number 02", "key number 03", true }
    };

    auto _content = [&_v]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: update authorization before and after HF26" );

      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), (alice) );
      executor->fund( "alice", 1000000 );
      executor->generate_block();

      executor->db_plugin->debug_update( [=]( database& db )
      {
        db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
        {
          wso.median_props.account_creation_fee = ASSET( "0.100 TESTS" );
        });
      });

      executor->generate_block();

      BOOST_TEST_MESSAGE( "Creating account bob with alice" );

      account_create_operation acc_create;
      acc_create.fee = ASSET( "0.100 TESTS" );
      acc_create.creator = "alice";
      acc_create.new_account_name = "bob";
      acc_create.owner = authority( 1, executor->generate_private_key( "bob_owner" ).get_public_key(), 1 );
      acc_create.active = authority( 1, executor->generate_private_key( "bob_active" ).get_public_key(), 1 );
      acc_create.posting = authority( 1, executor->generate_private_key( "bob_posting" ).get_public_key(), 1 );
      acc_create.memo_key = executor->generate_private_key( "bob_memo" ).get_public_key();
      acc_create.json_metadata = "";

      signed_transaction tx;
      tx.operations.push_back( acc_create );
      tx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      executor->sign( tx, alice_private_key );
      executor->db->push_transaction( tx, 0 );

      executor->vest( HIVE_INIT_MINER_NAME, "bob", asset( 1000, HIVE_SYMBOL ) );

      auto exec_update_op = [&]( const std::string& old_key,  const std::string& new_key, bool is_exception )
      {
        BOOST_TEST_MESSAGE("============update of authorization============");
        BOOST_TEST_MESSAGE( executor->db->head_block_time().to_iso_string().c_str() );

        signed_transaction tx;

        account_update_operation acc_update;
        acc_update.account = "bob";

        acc_update.owner = authority( 1, executor->generate_private_key( new_key ).get_public_key(), 1 );

        tx.operations.push_back( acc_update );
        tx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
        executor->sign( tx, executor->generate_private_key( old_key ) );

        if( is_exception )
        {
          HIVE_REQUIRE_THROW( executor->db->push_transaction( tx, 0 ), fc::assert_exception );
        }
        else
        {
          executor->db->push_transaction( tx, 0 );
        }

        executor->generate_block();
      };

      for( auto& data : _v )
      {
        exec_update_op( data.old_key, data.new_key, data.is_exception );
      }
    };

    execute_hardfork<25>( _content );

    _v = _v_hf26;

    execute_hardfork<26>( _content );

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( owner_update_limit )
{
  BOOST_TEST_MESSAGE( "Before HF26 changing an authorization was only once per hour, after HF26 is twice per hour" );

  bool hardfork_is_activated = false;

  fc::time_point_sec start_time = fc::variant( "2022-01-01T00:00:00" ).as< fc::time_point_sec >();

  auto previous_time    = start_time + fc::seconds(1);
  auto last_time        = start_time + fc::seconds(5);
  auto head_block_time  = start_time + fc::seconds(5);

  {
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( head_block_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( head_block_time, last_time ), false );

    head_block_time += fc::seconds(4);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( head_block_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( head_block_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( head_block_time, last_time ), true );
  }

  {
    last_time           = start_time + fc::seconds(1);
    head_block_time     = start_time + fc::seconds(1);

    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    head_block_time += fc::seconds(5);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(5);
    head_block_time = start_time + fc::seconds(5);

    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    head_block_time += fc::seconds(6);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time;
    last_time       = start_time + fc::seconds(1);
    head_block_time = start_time + fc::seconds(7);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );
  }

  hardfork_is_activated = true;

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(50);
    head_block_time = start_time + fc::seconds(50);

    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(7);
    head_block_time = start_time + fc::seconds(7);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    previous_time   -= fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(3);
    head_block_time = start_time + fc::seconds(9);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time + fc::seconds(3);
    last_time       = start_time + fc::seconds(3);
    head_block_time = start_time + fc::seconds(9);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );
  }

  {
    previous_time   = start_time + fc::seconds(3) - fc::seconds(1);
    last_time       = start_time + fc::seconds(3);
    head_block_time = start_time + fc::seconds(9);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(1);
    head_block_time = start_time + fc::seconds(7);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );
  }

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(1);
    head_block_time = start_time + fc::seconds(7) + fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time;
    last_time       = start_time + fc::seconds(1);
    head_block_time = start_time + fc::seconds(7);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time;
    last_time       = start_time;
    head_block_time = start_time + fc::seconds(7);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }
}

BOOST_AUTO_TEST_CASE( generate_block_size )
{
  try
  {
    uint32_t _trxs_count = 1;

    auto _content = [&_trxs_count]( ptr_hardfork_database_fixture& executor )
    {
      try
      {
        executor->db_plugin->debug_update( [=]( database& db )
        {
          db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
          {
            gpo.maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT;
          });
        });
        executor->generate_block();

        signed_transaction tx;
        tx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

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

        executor->sign( tx, executor->init_account_priv_key );
        executor->db->push_transaction( tx, 0 );

        // Second transaction, tx minus op is 78 (one less byte for operation vector size)
        // We need a 88 byte op. We need a 22 character memo (1 byte for length) 55 = 32 (old op) + 55 + 1
        op.memo = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ123";
        tx.clear();
        tx.operations.push_back( op );
        executor->sign( tx, executor->init_account_priv_key );
        executor->db->push_transaction( tx, 0 );

        executor->generate_block();

        // The last transfer should have been delayed due to size
        auto head_block = executor->db->fetch_block_by_number( executor->db->head_block_num() );
        BOOST_REQUIRE_EQUAL( head_block->transactions.size(), _trxs_count );
      }
      FC_LOG_AND_RETHROW()
    };

    hive::protocol::serialization_mode_controller::set_pack( hive::protocol::pack_type::legacy );
    execute_hardfork<25>( _content );

    _trxs_count = 2;

    execute_hardfork<26>( _content );

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( asset_raw_test )
{
  try
  {
    uint32_t _allow_hf26 = false;

    auto _content = [&_allow_hf26]( ptr_hardfork_database_fixture& executor )
    {

      auto _pack_nai_symbol = [](std::vector<char>& v, const asset_symbol_type& sym)
      {
        std::string _nai_str = sym.to_nai_string();

        uint32_t _val;

        if( sym == HIVE_SYMBOL )
        {
          _val = HIVE_ASSET_NUM_HIVE;
        }
        else if( sym == HBD_SYMBOL )
        {
          _val = HIVE_ASSET_NUM_HBD;
        }
        else if( sym == VESTS_SYMBOL )
        {
          _val = HIVE_ASSET_NUM_VESTS;
        }
        else
        {
          FC_ASSERT( false, "This method cannot serialize this symbol" );
        }

        std::vector<unsigned int> _bytes = { _val & 0x000000ff, (_val & 0x0000ff00) >> 8, (_val & 0x00ff0000) >> 16, (_val & 0xff000000) >> 24 };
        BOOST_CHECK_EQUAL( _bytes.size(), sizeof( _val ) );

        std::copy( _bytes.begin(), _bytes.end(), std::back_inserter( v ) );
      };

      auto _old_pack_symbol = [](std::vector<char>& v, const asset_symbol_type& sym)
      {
        if( sym == HIVE_SYMBOL )
        {
          v.push_back('\x03'); v.push_back('T' ); v.push_back('E' ); v.push_back('S' );
          v.push_back('T'   ); v.push_back('S' ); v.push_back('\0'); v.push_back('\0');
          // 03 54 45 53 54 53 00 00
        }
        else if( sym == HBD_SYMBOL )
        {
          v.push_back('\x03'); v.push_back('T' ); v.push_back('B' ); v.push_back('D' );
          v.push_back('\0'  ); v.push_back('\0'); v.push_back('\0'); v.push_back('\0');
          // 03 54 42 44 00 00 00 00
        }
        else if( sym == VESTS_SYMBOL )
        {
          v.push_back('\x06'); v.push_back('V' ); v.push_back('E' ); v.push_back('S' );
          v.push_back('T'   ); v.push_back('S' ); v.push_back('\0'); v.push_back('\0');
          // 06 56 45 53 54 53 00 00
        }
        else
        {
          FC_ASSERT( false, "This method cannot serialize this symbol" );
        }
        return;
      };

      auto _old_pack_asset = []( std::vector<char>& v, const asset& a, std::function<void( std::vector<char>& v, const asset_symbol_type& sym )> pack_symbol )
      {
        uint64_t x = uint64_t( a.amount.value );
        v.push_back( char( x & 0xFF ) );   x >>= 8;
        v.push_back( char( x & 0xFF ) );   x >>= 8;
        v.push_back( char( x & 0xFF ) );   x >>= 8;
        v.push_back( char( x & 0xFF ) );   x >>= 8;
        v.push_back( char( x & 0xFF ) );   x >>= 8;
        v.push_back( char( x & 0xFF ) );   x >>= 8;
        v.push_back( char( x & 0xFF ) );   x >>= 8;
        v.push_back( char( x & 0xFF ) );
        pack_symbol( v, a.symbol );
        return;
      };

      auto _hex_bytes = []( const std::vector<char>& obj ) -> std::string
      {
        std::vector<char> data = fc::raw::pack_to_vector( obj );
        std::ostringstream ss;
        static const char hexdigits[] = "0123456789abcdef";

        for( char c : data )
        {
          ss << hexdigits[((c >> 4) & 0x0F)] << hexdigits[c & 0x0F] << ' ';
        }
        return ss.str();
      };

      try
      {
        BOOST_CHECK( HBD_SYMBOL < HIVE_SYMBOL );
        BOOST_CHECK( HIVE_SYMBOL < VESTS_SYMBOL );

        // get a bunch of random bits
        fc::sha256 h = fc::sha256::hash("");

        std::vector< share_type > amounts;

        for( int i=0; i<64; i++ )
        {
          uint64_t s = (uint64_t(1) << i);
          uint64_t x = (h._hash[0] & (s-1)) | s;
          if( x >= HIVE_MAX_SHARE_SUPPLY )
            break;
          amounts.push_back( share_type( x ) );
        }
        // ilog( "h0:${h0}", ("h0", h._hash[0]) );

        std::vector< asset_symbol_type > symbols;

        symbols.push_back( HIVE_SYMBOL );
        symbols.push_back( HBD_SYMBOL   );
        symbols.push_back( VESTS_SYMBOL );

        for( const share_type& amount : amounts )
        {
          for( const asset_symbol_type& symbol : symbols )
          {
            // check raw::pack() works
            asset a = asset( amount, symbol );
            std::vector<char> v_old;

            if( _allow_hf26 )
              _old_pack_asset( v_old, a, _pack_nai_symbol );
            else
              _old_pack_asset( v_old, a, _old_pack_symbol );

            std::vector<char> v_cur = fc::raw::pack_to_vector(a);
            ilog( "${a} : ${d}", ("a", a)("d", _hex_bytes( v_old )) );
            ilog( "${a} : ${d}", ("a", a)("d", _hex_bytes( v_cur )) );
            BOOST_REQUIRE( v_cur == v_old );

            // check raw::unpack() works
            std::istringstream ss( std::string(v_cur.begin(), v_cur.end()) );
            asset a2;
            fc::raw::unpack( ss, a2 );
            BOOST_REQUIRE( a == a2 );

            // check conversion to JSON works
            //std::string json_old = old_json_asset(a);
            //std::string json_cur = fc::json::to_string(a);
            // ilog( "json_old: ${j}", ("j", json_old) );
            // ilog( "json_cur: ${j}", ("j", json_cur) );
            //BOOST_CHECK( json_cur == json_old );

            // check JSON serialization is symmetric
            std::string json_cur = fc::json::to_string(a);
            a2 = fc::json::from_string(json_cur).as< asset >();
            BOOST_REQUIRE( a == a2 );
          }
        }
      }
      FC_LOG_AND_RETHROW()
    };

    hive::protocol::serialization_mode_controller::set_pack( hive::protocol::pack_type::legacy );
    execute_hardfork<25>( _content );

    _allow_hf26 = true;

    execute_hardfork<26>( _content );

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif

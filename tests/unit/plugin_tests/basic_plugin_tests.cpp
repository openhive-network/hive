#if defined IS_TEST_NET && !defined ENABLE_STD_ALLOCATOR
#include <boost/test/unit_test.hpp>

#include <hive/plugins/account_by_key/account_by_key_objects.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_objects.hpp>
//#include <hive/plugins/block_log_info/block_log_info_objects.hpp>
#include <hive/plugins/market_history/market_history_plugin.hpp>
#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/reputation/reputation_objects.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/plugins/witness/witness_plugin_objects.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins;

BOOST_FIXTURE_TEST_SUITE( basic_plugin_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( debug_node_plugin_vests_hive_evaluation )
{
  auto _calculate_modifiers = [ this ]( share_type price_hive_base, share_type price_vests_quote,
                                        share_type total_hive, share_type total_vests,
                                        share_type hive_modifier, share_type vest_modifier
                                )
  {
    hive::protocol::asset _total_hive  = hive::protocol::asset( total_hive, HIVE_SYMBOL );
    hive::protocol::asset _total_vests = hive::protocol::asset( total_vests, VESTS_SYMBOL );
    hive::protocol::asset _hive_modifier;
    hive::protocol::asset _vest_modifier;
    hive::protocol::price _new_price   = hive::protocol::price( hive::protocol::asset( price_hive_base, HIVE_SYMBOL ), hive::protocol::asset( price_vests_quote, VESTS_SYMBOL ) );

    db_plugin->calculate_modifiers_according_to_new_price( _new_price, _total_hive, _total_vests, _hive_modifier, _vest_modifier );

    char _buffer[500];
    sprintf( _buffer, "_hive_modifier: ( %ld == %ld ) _vest_modifier: ( %ld == %ld )", _hive_modifier.amount.value, hive_modifier.value, _vest_modifier.amount.value, vest_modifier.value );
    BOOST_TEST_MESSAGE( _buffer );
    BOOST_CHECK( _hive_modifier == hive::protocol::asset( hive_modifier, HIVE_SYMBOL ) );
    BOOST_CHECK( _vest_modifier == hive::protocol::asset( vest_modifier, VESTS_SYMBOL ) );
  };

  _calculate_modifiers( 1/*price_hive_base*/, 500/*price_vests_quote*/, 13'044/*total_hive*/, 12'863'762'116'631/*total_vests*/, 25'727'511'189/*hive_modifier*/, 0/*vest_modifier*/ );

  _calculate_modifiers( 1/*price_hive_base*/, 1/*price_vests_quote*/, 1'000/*total_hive*/, 1'000/*total_vests*/, 0/*hive_modifier*/, 0/*vest_modifier*/ );

  _calculate_modifiers( 1/*price_hive_base*/, 2/*price_vests_quote*/, 1'000/*total_hive*/, 1'000/*total_vests*/, 0/*hive_modifier*/, 1'000/*vest_modifier*/ );
  _calculate_modifiers( 2/*price_hive_base*/, 1/*price_vests_quote*/, 1'000/*total_hive*/, 1'000/*total_vests*/, 1'000/*hive_modifier*/, 0/*vest_modifier*/ );

  _calculate_modifiers( 1/*price_hive_base*/, 1'000/*price_vests_quote*/, 1'000/*total_hive*/, 500'000/*total_vests*/, 0/*hive_modifier*/, 500'000/*vest_modifier*/ );
  _calculate_modifiers( 1'000/*price_hive_base*/, 1/*price_vests_quote*/, 500'000/*total_hive*/, 1'000/*total_vests*/, 500'000/*hive_modifier*/, 0/*vest_modifier*/ );

  _calculate_modifiers( 2/*price_hive_base*/, 400/*price_vests_quote*/, 1'000/*total_hive*/, 1'000'000/*total_vests*/, 4'000/*hive_modifier*/, 0/*vest_modifier*/ );

  _calculate_modifiers( 1/*price_hive_base*/, 500/*price_vests_quote*/, 800'000/*total_hive*/, 100'000'000/*total_vests*/, 0/*hive_modifier*/, 300'000'000/*vest_modifier*/ );

  _calculate_modifiers( 1/*price_hive_base*/, 3/*price_vests_quote*/, 1'000/*total_hive*/, 5'000/*total_vests*/, 666/*hive_modifier*/, 0/*vest_modifier*/ );
}

BOOST_AUTO_TEST_CASE( debug_node_plugin_state_modification )
{
  auto _dgpo_preparation = [ this ](  share_type total_hive, share_type total_vests,
                                      hive::protocol::asset& current_supply, hive::protocol::asset& virtual_supply )
  {
    generate_block();

    validate_database();

    auto& dgpo = db->get_dynamic_global_properties();

    char _buffer[500];

    sprintf( _buffer, "dgpo.total_vesting_fund_hive (%ld) dgpo.total_vesting_shares (%ld)", dgpo.total_vesting_fund_hive.amount.value, dgpo.total_vesting_shares.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    db_plugin->debug_update( [&]( database& db )
    {
      db.modify( dgpo, [ total_hive, total_vests ]( hive::chain::dynamic_global_property_object& p )
      {
        auto _total_hive  = hive::protocol::asset( total_hive, HIVE_SYMBOL );
        auto _total_vests = hive::protocol::asset( total_vests, VESTS_SYMBOL );

        p.total_vesting_fund_hive  += _total_hive;
        p.total_vesting_shares     += _total_vests;

        p.current_supply  += _total_hive;
        p.virtual_supply  += _total_hive;
      } );
    } );

    validate_database();

    current_supply  = dgpo.current_supply;
    virtual_supply  = dgpo.virtual_supply;
  };

  auto _check_state = [ this ]( share_type price_hive_base, share_type price_vests_quote,
                                const hive::protocol::asset& current_supply, const hive::protocol::asset& virtual_supply )
  {
    auto& dgpo = db->get_dynamic_global_properties();

    hive::protocol::asset _old_total_hive  = dgpo.total_vesting_fund_hive;
    hive::protocol::asset _old_total_vests = dgpo.total_vesting_shares;
    hive::protocol::asset _hive_modifier;
    hive::protocol::asset _vest_modifier;
    hive::protocol::price _new_price = hive::protocol::price( hive::protocol::asset( price_hive_base, HIVE_SYMBOL ), hive::protocol::asset( price_vests_quote, VESTS_SYMBOL ) );

    db_plugin->calculate_modifiers_according_to_new_price( _new_price, dgpo.total_vesting_fund_hive, dgpo.total_vesting_shares, _hive_modifier, _vest_modifier );

    db_plugin->debug_set_vest_price( _new_price );

    char _buffer[500];

    sprintf( _buffer, "dgpo.current_supply (%ld) == current_supply (%ld) + _hive_modifier (%ld)", dgpo.current_supply.amount.value, current_supply.amount.value, _hive_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.virtual_supply (%ld) == virtual_supply (%ld) + _hive_modifier (%ld)", dgpo.virtual_supply.amount.value, virtual_supply.amount.value, _hive_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.total_vesting_fund_hive (%ld) == _old_total_hive (%ld) + _hive_modifier (%ld)", dgpo.total_vesting_fund_hive.amount.value, _old_total_hive.amount.value, _hive_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.total_vesting_shares (%ld) == _old_total_vests (%ld) + _vest_modifier (%ld)", dgpo.total_vesting_shares.amount.value, _old_total_vests.amount.value, _vest_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    BOOST_CHECK( dgpo.current_supply          == current_supply + _hive_modifier );
    BOOST_CHECK( dgpo.virtual_supply          == virtual_supply + _hive_modifier );

    BOOST_CHECK( dgpo.total_vesting_fund_hive == _old_total_hive + _hive_modifier );
    BOOST_CHECK( dgpo.total_vesting_shares    == _old_total_vests + _vest_modifier );

    generate_block();

    validate_database();
  };

  {
    hive::protocol::asset _current_supply;
    hive::protocol::asset _virtual_supply;

    _dgpo_preparation(0/*total_hive*/, 0/*total_vests*/, _current_supply, _virtual_supply );
    _check_state( 1/*price_hive_base*/, 2'000'000'000/*price_vests_quote*/, _current_supply, _virtual_supply );
  }

}

BOOST_AUTO_TEST_CASE( plugin_object_size )
{
  BOOST_CHECK_EQUAL( sizeof( account_by_key::key_lookup_object ), 56u ); //3.4M, lasting, expected 3*account_object on average
  BOOST_CHECK_EQUAL( sizeof( account_by_key::key_lookup_index::MULTIINDEX_NODE_TYPE ), 120u );

  BOOST_CHECK_EQUAL( sizeof( account_history_rocksdb::volatile_operation_object ), 112u ); //temporary, at most as many as operations in reversible blocks
  BOOST_CHECK_EQUAL( sizeof( account_history_rocksdb::volatile_operation_index::MULTIINDEX_NODE_TYPE ), 176u );

  //BOOST_CHECK_EQUAL( sizeof( block_log_info::block_log_hash_state_object ), 0 );
  //BOOST_CHECK_EQUAL( sizeof( block_log_info::block_log_pending_message_object ), 0 );

  BOOST_CHECK_EQUAL( sizeof( market_history::bucket_object ), 96u //temporary, regulated amount, ~13k
#ifdef HIVE_ENABLE_SMT
    + 8u
#endif
  );
  BOOST_CHECK_EQUAL( sizeof( market_history::bucket_index::MULTIINDEX_NODE_TYPE ), 160u
#ifdef HIVE_ENABLE_SMT
    + 8u
#endif
  );
  BOOST_CHECK_EQUAL( sizeof( market_history::order_history_object ), 88u ); //permanent, growing with executed limit_order_object, 2.5M atm
  BOOST_CHECK_EQUAL( sizeof( market_history::order_history_index::MULTIINDEX_NODE_TYPE ), 152u );

  BOOST_CHECK_EQUAL( sizeof( rc::rc_resource_param_object ), 368u ); //singleton
  BOOST_CHECK_EQUAL( sizeof( rc::rc_resource_param_index::MULTIINDEX_NODE_TYPE ), 400u );
  BOOST_CHECK_EQUAL( sizeof( rc::rc_pool_object ), 176u ); //singleton
  BOOST_CHECK_EQUAL( sizeof( rc::rc_pool_index::MULTIINDEX_NODE_TYPE ), 208u );
  BOOST_CHECK_EQUAL( sizeof( rc::rc_stats_object ), 5520u //two objects
#ifdef HIVE_ENABLE_SMT
    + 616u
#endif
  );
  BOOST_CHECK_EQUAL( sizeof( rc::rc_stats_index::MULTIINDEX_NODE_TYPE ), 5552u
#ifdef HIVE_ENABLE_SMT
    + 616u
#endif
  );
  BOOST_CHECK_EQUAL( sizeof( rc::rc_pending_data ), 128u ); //singleton
  BOOST_CHECK_EQUAL( sizeof( rc::rc_pending_data_index::MULTIINDEX_NODE_TYPE ), 160u );
  BOOST_CHECK_EQUAL( sizeof( rc::rc_account_object ), 80u ); //permanent, as many as account_object, 1.3M atm
  BOOST_CHECK_EQUAL( sizeof( rc::rc_account_index::MULTIINDEX_NODE_TYPE ), 144u );
  BOOST_CHECK_EQUAL( sizeof( rc::rc_usage_bucket_object ), 48u ); //always HIVE_RC_WINDOW_BUCKET_COUNT objects
  BOOST_CHECK_EQUAL( sizeof( rc::rc_usage_bucket_index::MULTIINDEX_NODE_TYPE ), 112u );
  BOOST_CHECK_EQUAL( sizeof( rc::rc_direct_delegation_object ), 24u ); //lasting, most likely more popular than regular delegation so count should be expected in the millions
  BOOST_CHECK_EQUAL( sizeof( rc::rc_direct_delegation_index::MULTIINDEX_NODE_TYPE ), 88u );
  BOOST_CHECK_EQUAL( sizeof( rc::rc_expired_delegation_object ), 16u ); //temporary, none most of the time (only used in very specific case)
  BOOST_CHECK_EQUAL( sizeof( rc::rc_expired_delegation_index::MULTIINDEX_NODE_TYPE ), 80u );

  BOOST_CHECK_EQUAL( sizeof( reputation::reputation_object ), 32u ); //lasting, as many as account_object, 1.3M atm
  BOOST_CHECK_EQUAL( sizeof( reputation::reputation_index::MULTIINDEX_NODE_TYPE ), 96u );

  BOOST_CHECK_EQUAL( sizeof( transaction_status::transaction_status_object ), 40u ); //temporary, depends on tracking flag, cuts out data from too old blocks
  BOOST_CHECK_EQUAL( sizeof( transaction_status::transaction_status_index::MULTIINDEX_NODE_TYPE ), 136u );

  BOOST_CHECK_EQUAL( sizeof( witness::witness_custom_op_object ), 32u ); //temporary, at most as many as account_object affected by custom ops in single block
  BOOST_CHECK_EQUAL( sizeof( witness::witness_custom_op_index::MULTIINDEX_NODE_TYPE ), 96u );
}

BOOST_AUTO_TEST_SUITE_END()
#endif

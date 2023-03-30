#if defined IS_TEST_NET && !defined ENABLE_STD_ALLOCATOR
#include <boost/test/unit_test.hpp>

#include <hive/plugins/account_by_key/account_by_key_objects.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_objects.hpp>
#include <hive/plugins/block_log_info/block_log_info_objects.hpp>
#include <hive/plugins/follow/follow_objects.hpp>
#include <hive/plugins/market_history/market_history_plugin.hpp>
#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/reputation/reputation_objects.hpp>
#include <hive/plugins/tags/tags_plugin.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/plugins/witness/witness_plugin_objects.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <hive/chain/util/decoded_types_data_storage.hpp>

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
  auto _check_state = [ this ]( share_type price_hive_base, share_type price_vests_quote )
  {
    generate_block();
    validate_database();

    auto& _old_dgpo = db->get_dynamic_global_properties();

    char _buffer[500];

    sprintf( _buffer, "dgpo.total_vesting_fund_hive (%ld) dgpo.total_vesting_shares (%ld)", _old_dgpo.total_vesting_fund_hive.amount.value, _old_dgpo.total_vesting_shares.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    hive::protocol::asset _old_current_supply = _old_dgpo.current_supply;
    hive::protocol::asset _old_virtual_supply = _old_dgpo.virtual_supply;

    hive::protocol::asset _old_total_hive  = _old_dgpo.total_vesting_fund_hive;
    hive::protocol::asset _old_total_vests = _old_dgpo.total_vesting_shares;
    hive::protocol::asset _hive_modifier;
    hive::protocol::asset _vest_modifier;
    hive::protocol::price _new_price = hive::protocol::price( hive::protocol::asset( price_hive_base, HIVE_SYMBOL ), hive::protocol::asset( price_vests_quote, VESTS_SYMBOL ) );

    db_plugin->calculate_modifiers_according_to_new_price( _new_price, _old_dgpo.total_vesting_fund_hive, _old_dgpo.total_vesting_shares, _hive_modifier, _vest_modifier );

    db_plugin->debug_set_vest_price( _new_price );

    auto& _dgpo = db->get_dynamic_global_properties();

    sprintf( _buffer, "dgpo.current_supply (%ld) == _old_current_supply (%ld) + _hive_modifier (%ld)", _dgpo.current_supply.amount.value, _old_current_supply.amount.value, _hive_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.virtual_supply (%ld) == _old_virtual_supply (%ld) + _hive_modifier (%ld)", _dgpo.virtual_supply.amount.value, _old_virtual_supply.amount.value, _hive_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.total_vesting_fund_hive (%ld) == _old_total_hive (%ld) + _hive_modifier (%ld)", _dgpo.total_vesting_fund_hive.amount.value, _old_total_hive.amount.value, _hive_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.total_vesting_shares (%ld) == _old_total_vests (%ld) + _vest_modifier (%ld)", _dgpo.total_vesting_shares.amount.value, _old_total_vests.amount.value, _vest_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    BOOST_CHECK( _dgpo.current_supply          == _old_current_supply + _hive_modifier );
    BOOST_CHECK( _dgpo.virtual_supply          == _old_virtual_supply + _hive_modifier );

    BOOST_CHECK( _dgpo.total_vesting_fund_hive == _old_total_hive + _hive_modifier );
    BOOST_CHECK( _dgpo.total_vesting_shares    == _old_total_vests + _vest_modifier );

    generate_block();
    validate_database();
  };

  {
    BOOST_TEST_MESSAGE("hive_modifier == 0 and vest_modifier > 0");
    _check_state( 1/*price_hive_base*/, 2'000'000'000/*price_vests_quote*/ );
  }

  {
    BOOST_TEST_MESSAGE("hive_modifier > 0 and vest_modifier == 0");
    _check_state( 1/*price_hive_base*/, 10'000/*price_vests_quote*/ );
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

template <typename T>
std::string_view get_decoded_type_checksum(hive::chain::util::decoded_types_data_storage& dtds)
{
  dtds.register_new_type<T>();
  return dtds.get_decoded_type_checksum<T>();
}

BOOST_AUTO_TEST_CASE( plugin_object_checksum )
{
  util::decoded_types_data_storage dtds;

  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::account_by_key::key_lookup_object>(dtds), "21368c880381c5684dee87750479de271a2bfe76" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::account_history_rocksdb::volatile_operation_object>(dtds), "4c92b72006ceac2c3e984eb1978390947f3c41df" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::block_log_info::block_log_hash_state_object>(dtds), "6173e5b82a43b542b8374311dceba299c06685c8" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::block_log_info::block_log_pending_message_object>(dtds), "0660b7e1f7cb9767dbcc2defc736ff05bd067275" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::follow_object>(dtds), "dea18af8e14a666e16c235417cd3ebbf01f32ea9" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::feed_object>(dtds), "9fc01a4fab5f59c850109c281eaedadbeee73d8d" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::blog_object>(dtds), "21fb3738991bf0849e58462b8aa476f58e29c5ff" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::blog_author_stats_object>(dtds), "00913248b32659781431251262291e2e76564c48" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::reputation_object>(dtds), "2e27d233bd0c71cfe7429787e2913597e1cabbff" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::follow_count_object>(dtds), "c10d9d3107af456a1ad824860388c341b518a170" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::market_history::order_history_object>(dtds), "b43a086969ad26b9347ce2a876af17fdaf4fa0e8" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::rc::rc_resource_param_object>(dtds), "a283de5a95d6026c37e266f868836a4728cd1efd" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::rc::rc_pool_object>(dtds), "0de3692294cd21cdc895748f8d77cabc00cc11eb" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::rc::rc_pending_data>(dtds), "d01fb8e966668911fcccfeb69b2c409c4aa6647a" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::rc::rc_account_object>(dtds), "95a7ff2b1c6582f52c3996e6f7052bfc54bd0505" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::rc::rc_direct_delegation_object>(dtds), "4125192524090b813c6518262992af4248a368a9" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::rc::rc_usage_bucket_object>(dtds), "506973e9daf1e436612f27518c048c2e74e72603" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::reputation::reputation_object>(dtds), "dd76a68d64569b1a4b02d0910431e9c902656f11" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::tags::tag_object>(dtds), "02c3f2f3c40de66efcc5f1033f6d62d1aef57961" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::tags::tag_stats_object>(dtds), "05b570c3396c8176cba9abf9c7c0baaf93e8135e" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::tags::author_tag_stats_object>(dtds), "c029d9bb0a30c846d47a4b4b9742bc55325fe093" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::transaction_status::transaction_status_object>(dtds), "20e988b5505821be0cb75a57a89999f4c168d00b" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::witness::witness_custom_op_object>(dtds), "8f227484c1a87918538c8727c916acf48bdbb965" );

  #ifdef HIVE_ENABLE_SMT
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::market_history::bucket_object>(dtds), "77f71ebcdf8ff22735400f3809b1a9884611ea8b" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::rc::rc_stats_object>(dtds), "2c5a48c0db94641b8a862b58a7783412659e0570" );
  #else
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::market_history::bucket_object>(dtds), "c961c3886afa159b6bf38586d1b22500d31d6ce3" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::rc::rc_stats_object>(dtds), "e49e5008e8ee2d40fee07a5b8840403372f8d0bc" );
  #endif
}

BOOST_AUTO_TEST_SUITE_END()
#endif

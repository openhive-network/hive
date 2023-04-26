#if defined IS_TEST_NET && !defined ENABLE_STD_ALLOCATOR
#include <boost/test/unit_test.hpp>

#include <hive/plugins/account_by_key/account_by_key_objects.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_objects.hpp>
#include <hive/plugins/block_log_info/block_log_info_objects.hpp>
#include <hive/plugins/follow/follow_objects.hpp>
#include <hive/plugins/market_history/market_history_plugin.hpp>
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

  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::account_by_key::key_lookup_object>(dtds), "b15664037aaf526e9abf4e295abc58bac301c813" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::account_history_rocksdb::volatile_operation_object>(dtds), "009775c2d4bf3384fa99c350674151b5c5c46b2a" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::block_log_info::block_log_hash_state_object>(dtds), "1ad502e939386f07fb513d48c8c07f9ff5a76a6a" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::block_log_info::block_log_pending_message_object>(dtds), "b7da18e0b992b242d903ac255ca3023151db5e16" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::follow_object>(dtds), "ec79acbc66b8210e1e6cd31c5edc8fc5d86618fe" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::feed_object>(dtds), "e81464ef261de05536c9ebfb72fbd401400516e3" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::blog_object>(dtds), "f937a7645c6d33f37ddc38b1d72546abc6724aa5" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::blog_author_stats_object>(dtds), "58e3ac97c4bc94439018638905b3bc9ff14fb7ce" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::reputation_object>(dtds), "49ee99bdff6ef39bc07ec1c8512a2e8818c74c8b" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::follow::follow_count_object>(dtds), "e1411fd1ee8d2fff1f8e825162e93728edd35bbf" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::market_history::order_history_object>(dtds), "d44984762f037d0a93007dfc3f172c0cca5cf8f2" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::reputation::reputation_object>(dtds), "cacdc6e0294f4098f4cef0c3e0bc06e4d7ede488" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::tags::tag_object>(dtds), "3b2e46494852fc71120a3da6d4530083ed046a3f" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::tags::tag_stats_object>(dtds), "0de2c00c5633f9c1c7d79b5de15045b9016c772f" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::tags::author_tag_stats_object>(dtds), "1fc079af51f6655c554ea2d30495802f626540b2" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::transaction_status::transaction_status_object>(dtds), "cb9ceb3c9d94912d0e5326d6ebfcd6110bd9c953" );
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::witness::witness_custom_op_object>(dtds), "685bb79eb173fc063846cf12f938c26125f7099c" );

  #ifdef HIVE_ENABLE_SMT
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::market_history::bucket_object>(dtds), "0ed6488aad10a231f02e7e58a5f7733362821c72" );
  #else
  BOOST_CHECK_EQUAL( get_decoded_type_checksum<hive::plugins::market_history::bucket_object>(dtds), "fc8f56ad056d2594e97aac6e7e618007a1bfecfb" );
  #endif
}

BOOST_AUTO_TEST_SUITE_END()
#endif

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

BOOST_AUTO_TEST_CASE( plugin_object_checksum )
{
  util::decoded_types_data_storage& dtds_instance = util::decoded_types_data_storage::get_instance();

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
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_direct_delegation_object>().str(), fc::ripemd160("c4f36c482e5fc6d63bd1f8a128f8510718f9a759").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_usage_bucket_object>().str(), fc::ripemd160("891e927cfce3c8780e2fc2fd31cc3235168273e0").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::reputation::reputation_object>().str(), fc::ripemd160("e6ca2e8a612c4b357340c7e37c7cecd03a8fc51f").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::tag_object>().str(), fc::ripemd160("fd017b495cdf4ff2e885735f8343329be25851df").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::tag_stats_object>().str(), fc::ripemd160("00342887e23a1b3a2721dfc8b1c3334bed7719ec").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::tags::author_tag_stats_object>().str(), fc::ripemd160("ceeaeead6e51fdff8a7554daa61ee3db1d6d7b4e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::transaction_status::transaction_status_object>().str(), fc::ripemd160("447d1026c3cc02dc47f9afb2282184274cd5e830").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::witness::witness_custom_op_object>().str(), fc::ripemd160("001551883f460560a52593e66e0f2aa25f84369c").str() );

  #ifdef HIVE_ENABLE_SMT
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::market_history::bucket_object>().str(), fc::ripemd160("4dfaebaf00d594efafd1137845e108590a3abbc3").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_stats_object>().str(), fc::ripemd160("3d213282743e5f4da31b39e67a68b31479d78a70").str() );
  #else
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::market_history::bucket_object>().str(), fc::ripemd160("64f7f320e15f36e252458c47191a2d4057d57d4e").str() );
  BOOST_CHECK_EQUAL( dtds_instance.get_decoded_type_checksum<hive::plugins::rc::rc_stats_object>().str(), fc::ripemd160("64b46c1e171ea9e663cccb9a7d83f553c52c02d7").str() );
  #endif
}

BOOST_AUTO_TEST_SUITE_END()
#endif

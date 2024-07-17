#if defined IS_TEST_NET && !defined ENABLE_STD_ALLOCATOR
#include <boost/test/unit_test.hpp>

#include <hive/plugins/account_by_key/account_by_key_objects.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_objects.hpp>
#include <hive/plugins/block_log_info/block_log_info_objects.hpp>
#include <hive/plugins/market_history/market_history_plugin.hpp>
#include <hive/plugins/reputation/reputation_objects.hpp>
#include <hive/plugins/tags/tags_plugin.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/plugins/witness/witness_plugin_objects.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

#include <hive/chain/util/decoded_types_data_storage.hpp>

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins;

BOOST_FIXTURE_TEST_SUITE( basic_plugin_tests, clean_database_fixture )

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

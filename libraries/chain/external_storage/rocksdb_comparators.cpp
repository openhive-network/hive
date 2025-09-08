#include <hive/chain/external_storage/rocksdb_comparators.hpp>
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>

#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>
#include <hive/chain/external_storage/utilities.hpp>
#include <hive/chain/external_storage/types.hpp>

#include <appbase/application.hpp>

#include <rocksdb/utilities/object_registry.h>
#include <rocksdb/utilities/options_type.h>
#include <rocksdb/status.h>
#include <mutex>

namespace hive { namespace chain {

void registerHiveComparators() {
  static std::once_flag registered;
  std::call_once(registered, []() {
    auto& library = *::rocksdb::ObjectLibrary::Default();

    ilog("Registering custom comparators with RocksDB ObjectLibrary");

    library.AddFactory<const ::rocksdb::Comparator>(
        "hive::chain::HashComparator const*",
        [](const std::string& /*uri*/, std::unique_ptr<const ::rocksdb::Comparator>* /*guard*/, std::string* /*errmsg*/) {
          return by_Hash_Comparator();
        });

    library.AddFactory<const ::rocksdb::Comparator>(
        "hive::chain::by_id_ComparatorImpl const*",
        [](const std::string& /*uri*/, std::unique_ptr<const ::rocksdb::Comparator>* /*guard*/, std::string* /*errmsg*/) {
          return by_id_Comparator();
        });

    library.AddFactory<const ::rocksdb::Comparator>(
        "hive::chain::op_by_block_num_ComparatorImpl const*",
        [](const std::string& /*uri*/, std::unique_ptr<const ::rocksdb::Comparator>* /*guard*/, std::string* /*errmsg*/) {
          return op_by_block_num_Comparator();
        });

    library.AddFactory<const ::rocksdb::Comparator>(
        "hive::chain::by_account_name_ComparatorImpl const*",
        [](const std::string& /*uri*/, std::unique_ptr<const ::rocksdb::Comparator>* /*guard*/, std::string* /*errmsg*/) {
          return by_account_name_Comparator();
        });

    library.AddFactory<const ::rocksdb::Comparator>(
        "hive::chain::ah_op_by_id_ComparatorImpl const*",
        [](const std::string& /*uri*/, std::unique_ptr<const ::rocksdb::Comparator>* /*guard*/, std::string* /*errmsg*/) {
          return ah_op_by_id_Comparator();
        });

    library.AddFactory<const ::rocksdb::Comparator>(
        "hive::chain::TransactionIdComparator const*",
        [](const std::string& /*uri*/, std::unique_ptr<const ::rocksdb::Comparator>* /*guard*/, std::string* /*errmsg*/) {
          return by_txId_Comparator();
        });

    ilog("Successfully registered all custom comparators with RocksDB ObjectLibrary");
    });
  }

}}

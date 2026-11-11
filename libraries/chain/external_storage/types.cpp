#include <hive/chain/external_storage/types.hpp>

#include <rocksdb/utilities/object_registry.h>
#include <rocksdb/utilities/options_type.h>
#include <rocksdb/status.h>

#include <mutex>

namespace hive { namespace chain {

const Comparator* by_Hash_Comparator()
{
  static HashComparator c;
  return &c;
}

const Comparator* by_id_Comparator()
{
  static by_id_ComparatorImpl c;
  return &c;
}

const Comparator* op_by_block_num_Comparator()
{
  static op_by_block_num_ComparatorImpl c;
  return &c;
}

const Comparator* by_account_name_Comparator()
{
  static by_account_name_ComparatorImpl c;
  return &c;
}

const Comparator* ah_op_by_id_Comparator()
{
  static ah_op_by_id_ComparatorImpl c;
  return &c;
}

const Comparator* by_txId_Comparator()
{
  static TransactionIdComparator c;
  return &c;
}

const Comparator* snapshot_index_Comparator()
{
  static snapshot_index_ComparatorImpl c;
  return &c;
}

bool CachableWriteBatch::getAHInfo(const account_name_type& name, account_history_info* ahInfo) const
{
  auto fi = _ahInfoCache.find(name);
  if(fi != _ahInfoCache.end())
  {
    *ahInfo = fi->second;
    return true;
  }

  ah_info_by_name_slice_t key(name.data);
  PinnableSlice buffer;
  auto s = _storage->Get(ReadOptions(), _columnHandles[Columns::AH_INFO_BY_NAME], key, &buffer);
  if(s.ok())
  {
    load(*ahInfo, buffer.data(), buffer.size());
    return true;
  }

  FC_ASSERT(s.IsNotFound());
  return false;
}

void CachableWriteBatch::putAHInfo(const account_name_type& name, const account_history_info& ahInfo)
{
  _ahInfoCache[name] = ahInfo;
  auto serializeBuf = dump(ahInfo);
  ah_info_by_name_slice_t nameSlice(name.data);
  auto s = Put(_columnHandles[Columns::AH_INFO_BY_NAME], nameSlice, Slice(serializeBuf.data(), serializeBuf.size()));
  checkStatus(s);
}

void CachableWriteBatch::Clear()
{
  _ahInfoCache.clear();
  WriteBatch::Clear();
}

void registerHiveComparators() {
  static std::once_flag registered;
  std::call_once(registered, []() {
    auto library = std::make_shared<::rocksdb::ObjectLibrary>("hive");

    // RocksDB stores Name() in every SST and OPTIONS file it writes, and looks the comparator up
    // by that name when it reads them back without one given (sst_dump, LoadLatestOptions).
    for(const Comparator* comparator : { by_Hash_Comparator(), by_id_Comparator(), op_by_block_num_Comparator(),
      by_account_name_Comparator(), ah_op_by_id_Comparator(), by_txId_Comparator(), snapshot_index_Comparator() })
    {
      library->AddFactory<const Comparator>( comparator->Name(),
        [comparator](const std::string& /*uri*/, std::unique_ptr<const Comparator>* /*guard*/, std::string* /*errmsg*/) {
          return comparator;
        });
    }

    ::rocksdb::ObjectRegistry::Default()->AddLibrary(library);
  });
}

} } // hive::chain

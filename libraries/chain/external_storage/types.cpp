#include <hive/chain/external_storage/types.hpp>

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

const Comparator* by_time_account_name_pair_Comparator()
{
  static time_account_name_pair_ComparatorImpl c;
  return &c;
}

const Comparator* by_time_account_id_pair_Comparator()
{
  static time_account_id_pair_ComparatorImpl c;
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

  account_name_slice_t key(name.data);
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
  account_name_slice_t nameSlice(name.data);
  auto s = Put(_columnHandles[Columns::AH_INFO_BY_NAME], nameSlice, Slice(serializeBuf.data(), serializeBuf.size()));
  checkStatus(s);
}

void CachableWriteBatch::Clear()
{
  _ahInfoCache.clear();
  WriteBatch::Clear();
}

} } // hive::chain

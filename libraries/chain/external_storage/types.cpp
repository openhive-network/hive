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

} } // hive::chain

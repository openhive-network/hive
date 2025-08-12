#pragma once

#include <hive/chain/external_storage/rocksdb_column_family_iterator.hpp>

namespace hive { namespace chain {

template<typename ByIndex>
class rocksdb_iterator_provider
{
  public:

    rocksdb_account_iterator::ptr get_iterator( rocksdb_account_column_family_iterator_provider::ptr& ){ return rocksdb_account_iterator::ptr(); };
};

template<>
class rocksdb_iterator_provider<by_next_vesting_withdrawal>
{
  public:

    static rocksdb_account_iterator::ptr get_iterator( rocksdb_account_column_family_iterator_provider::ptr& provider )
    {
      return std::make_shared<rocksdb_column_family_iterator_by_next_vesting_withdrawal>( ColumnTypes::ACCOUNT_BY_NEXT_VESTING_WITHDRAWAL, provider );
    }

};

}}
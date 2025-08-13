#pragma once

#include <hive/chain/external_storage/rocksdb_account_iterator.hpp>

namespace hive { namespace chain {

class rocksdb_column_family_iterator: public rocksdb_account_iterator
{
  private:

    ColumnTypes column_type;

    std::unique_ptr<::rocksdb::Iterator> it;

  public:

    rocksdb_column_family_iterator( ColumnTypes _column_type, rocksdb_account_column_family_iterator_provider::ptr& _provider );

    std::shared_ptr<account_object> begin() override;

    void next() override;
    bool end() override;
};

class rocksdb_column_family_iterator_by_next_vesting_withdrawal: public rocksdb_column_family_iterator
{
  private:

    ColumnTypes column_type;

    std::unique_ptr<::rocksdb::Iterator> it;

  public:

    rocksdb_column_family_iterator_by_next_vesting_withdrawal( ColumnTypes _column_type, rocksdb_account_column_family_iterator_provider::ptr& _provider );
    std::shared_ptr<account_object> get() override;
};

}}
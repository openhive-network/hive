#pragma once

#include <hive/chain/external_storage/account_item.hpp>
#include <hive/chain/external_storage/types.hpp>

namespace hive { namespace chain {

class rocksdb_account_iterator
{
  public:

    using ptr = std::shared_ptr<rocksdb_account_iterator>;

    virtual std::shared_ptr<account_object> begin() = 0;
    virtual std::shared_ptr<account_object> get() = 0;
    virtual void next() = 0;
    virtual bool end() = 0;
};

class rocksdb_account_column_family_iterator_provider
{
  public:

    using ptr = std::shared_ptr<rocksdb_account_column_family_iterator_provider>;

    virtual std::unique_ptr<::rocksdb::Iterator> create_column_family_iterator( ColumnTypes column_type ) = 0;
};

}}

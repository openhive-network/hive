#pragma once

#include <hive/chain/external_storage/account_item.hpp>
#include <hive/chain/external_storage/types.hpp>

namespace hive { namespace chain {

class rocksdb_account_iterator
{
  public:

    using ptr = std::shared_ptr<rocksdb_account_iterator>;

    virtual account begin() = 0;
    virtual account get() = 0;
    virtual void next() = 0;
    virtual bool end() = 0;
};

class rocksdb_account_iterator_provider
{
  public:

    using ptr = std::shared_ptr<rocksdb_account_iterator>;

    virtual rocksdb_account_iterator_provider::ptr get_iterator() = 0;
};

}}

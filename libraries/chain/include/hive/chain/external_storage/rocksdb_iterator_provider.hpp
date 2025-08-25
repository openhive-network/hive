#pragma once

#include <hive/chain/external_storage/rocksdb_column_family_iterator.hpp>
#include <hive/chain/external_storage/external_storage_provider.hpp>

namespace hive { namespace chain {

template<typename ByIndex>
class rocksdb_iterator_provider
{
};

template<>
class rocksdb_iterator_provider<by_next_vesting_withdrawal>
{
  public:

    static rocksdb_account_iterator::ptr get_iterator( const chainbase::database& db, rocksdb_account_column_family_iterator_provider::ptr provider, external_storage_reader_writer::ptr reader )
    {
      return std::make_shared<rocksdb_column_family_iterator_by_next_vesting_withdrawal>( db, ColumnTypes::ACCOUNT_BY_NEXT_VESTING_WITHDRAWAL, provider, reader );
    }

};

template<>
class rocksdb_iterator_provider<by_delayed_voting>
{
  public:

    static rocksdb_account_iterator::ptr get_iterator( const chainbase::database& db, rocksdb_account_column_family_iterator_provider::ptr provider, external_storage_reader_writer::ptr reader )
    {
      return std::make_shared<rocksdb_column_family_iterator_by_delayed_voting>( db, ColumnTypes::ACCOUNT_BY_DELAYED_VOTING, provider, reader );
    }

};

}}
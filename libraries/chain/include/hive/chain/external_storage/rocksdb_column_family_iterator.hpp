#pragma once

#include <hive/chain/external_storage/rocksdb_account_iterator.hpp>
#include <hive/chain/external_storage/external_storage_provider.hpp>

namespace hive { namespace chain {

class rocksdb_column_family_iterator: public rocksdb_account_iterator
{
  protected:

    const chainbase::database& db;

    std::unique_ptr<::rocksdb::Iterator> it;

    external_storage_reader_writer::ptr reader;

    std::shared_ptr<account_object> get_account( const std::string& message, const account_name_type& account_name );

  public:

    rocksdb_column_family_iterator( const chainbase::database& db, ColumnTypes _column_type,
                  rocksdb_account_column_family_iterator_provider::ptr provider, external_storage_reader_writer::ptr reader );

    void begin() override;

    void next() override;
    bool end() override;
};

class rocksdb_column_family_iterator_by_next_vesting_withdrawal: public rocksdb_column_family_iterator
{
  public:

    rocksdb_column_family_iterator_by_next_vesting_withdrawal( const chainbase::database& db, ColumnTypes _column_type,
                  rocksdb_account_column_family_iterator_provider::ptr _provider, external_storage_reader_writer::ptr reader );

    std::shared_ptr<account_object> get() override;
};

class rocksdb_column_family_iterator_by_delayed_voting: public rocksdb_column_family_iterator
{
  public:

    rocksdb_column_family_iterator_by_delayed_voting( const chainbase::database& db, ColumnTypes _column_type,
                  rocksdb_account_column_family_iterator_provider::ptr _provider, external_storage_reader_writer::ptr reader );

    std::shared_ptr<account_object> get() override;
};

class rocksdb_column_family_iterator_by_governance_vote_expiration_ts: public rocksdb_column_family_iterator
{
  public:

    rocksdb_column_family_iterator_by_governance_vote_expiration_ts( const chainbase::database& db, ColumnTypes _column_type,
                  rocksdb_account_column_family_iterator_provider::ptr _provider, external_storage_reader_writer::ptr reader );

    std::shared_ptr<account_object> get() override;
};

}}
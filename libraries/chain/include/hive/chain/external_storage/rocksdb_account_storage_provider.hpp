#pragma once

#include <hive/chain/external_storage/rocksdb_base_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_account_iterator.hpp>

#include <rocksdb/options.h>
#include <rocksdb/slice.h>

#include <boost/core/demangle.hpp>
#include <boost/filesystem/path.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/filesystem.hpp>

namespace hive { namespace chain {

class rocksdb_account_storage_provider: public rocksdb_account_column_family_iterator_provider, public rocksdb_base_storage_provider
{
  private:

    ColumnDefinitions prepareColumnDefinitions(bool addDefaultColumn) override;

  public:

    using ptr = std::shared_ptr<rocksdb_account_storage_provider>;

    rocksdb_account_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    ~rocksdb_account_storage_provider() override{}

    void save( ColumnTypes column_type, const Slice& key, const Slice& value ) override;
    bool read( ColumnTypes column_type, const Slice& key, PinnableSlice& value ) override;
    void remove( ColumnTypes column_type, const Slice& key ) override;

    std::unique_ptr<::rocksdb::Iterator> create_column_family_iterator( ColumnTypes column_type ) override;
};

}}


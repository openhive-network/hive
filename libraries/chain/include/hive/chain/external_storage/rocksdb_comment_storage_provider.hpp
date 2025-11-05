#pragma once

#include <hive/chain/external_storage/rocksdb_base_storage_provider.hpp>

#include <hive/chain/database.hpp>

#include <rocksdb/options.h>
#include <rocksdb/slice.h>

#include <boost/core/demangle.hpp>
#include <boost/filesystem/path.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/filesystem.hpp>

namespace hive { namespace chain {

class rocksdb_comment_storage_provider: public rocksdb_base_storage_provider, public external_storage_reader_writer
{
  private:

    ColumnDefinitions prepareColumnDefinitions(bool addDefaultColumn) override;

  public:

    using ptr = std::shared_ptr<rocksdb_comment_storage_provider>;

    rocksdb_comment_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    ~rocksdb_comment_storage_provider() override{}

    void save( ColumnTypes column_type, const Slice& key, const Slice& value ) override;
    bool read( ColumnTypes column_type, const Slice& key, PinnableSlice& value ) override;
    void remove( ColumnTypes column_type, const Slice& key ) override { /*Not supported here.*/ };

    void put_entity( ColumnTypes column_type, const Slice& key, const WideColumns& wide_columns ) override { /*Not supported here.*/ };
    bool get_entity( ColumnTypes column_type, const Slice& key, PinnableWideColumns& wide_columns ) override { /*Not supported here.*/ return true; };

    void compaction() override { /*Not supported here.*/ };
};

}}


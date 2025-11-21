#pragma once

#include <hive/chain/external_storage/external_storage_snapshot.hpp>
#include <hive/chain/external_storage/external_basic_provider.hpp>
#include <hive/chain/database.hpp>

#include <rocksdb/db.h>

#include<string>

#include <boost/filesystem.hpp>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

using ::rocksdb::DB;

class rocksdb_snapshot: public external_storage_snapshot
{
  private:

    std::string _name;
    std::string _storage_name;

    database& _mainDb;

    const bfs::path _storagePath;

    external_basic_provider::ptr _provider;

  public:

    rocksdb_snapshot( std::string name, std::string storage_name, database& db,
                      const bfs::path& storage_path, const external_basic_provider::ptr& provider );

    void save_snapshot( const prepare_snapshot_supplement_notification& note ) override;
    void load_snapshot( const load_snapshot_supplement_notification& note ) override;

};

}}

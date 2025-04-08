#pragma once

#include <hive/chain/external_storage/external_storage_snapshot.hpp>
#include <hive/chain/external_storage/external_storage_provider.hpp>

#include <rocksdb/db.h>

#include<string>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

using ::rocksdb::DB;

class rocksdb_snapshot: public external_storage_snapshot
{
  private:

    std::string _name;
    std::string _storage_name;

    chain::database& _mainDb;

    const hive::chain::abstract_plugin& _plugin;

    const bfs::path _storagePath;

    external_snapshot_storage_provider::ptr _provider;

  public:

    rocksdb_snapshot( std::string name, std::string storage_name, const hive::chain::abstract_plugin& plugin, chain::database& db,
                      const bfs::path& storage_path, const external_snapshot_storage_provider::ptr& provider );

    void supplement_snapshot( const hive::chain::prepare_snapshot_supplement_notification& note ) override;
    void load_additional_data_from_snapshot( const hive::chain::load_snapshot_supplement_notification& note ) override;

};

}}

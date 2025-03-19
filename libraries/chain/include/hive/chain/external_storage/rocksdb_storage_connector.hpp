#pragma once

#include <hive/chain/external_storage/external_storage_connector.hpp>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

class rocksdb_storage_connector: public external_storage_connector
{
  private:

    void on_post_apply_operation( const operation_notification& note );
    void on_irreversible_block( uint32_t block_num );

    boost::signals2::connection _post_apply_operation_conn;
    boost::signals2::connection _on_irreversible_block_conn;

  public:

    rocksdb_storage_connector( const abstract_plugin& plugin, database& db, const bfs::path& path );
    ~rocksdb_storage_connector() override;
};

}}

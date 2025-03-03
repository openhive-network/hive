#pragma once

#include <hive/chain/external_storage/external_storage_connector.hpp>

#include<hive/chain/database.hpp>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

class rocksdb_storage_connector: public external_storage_connector
{
  private:

    database& db;

    void on_post_apply_operation( const operation_notification& note );
    void on_irreversible_block( uint32_t block_num );
    void on_remove_comment_cashout( const remove_comment_cashout_notification& note );

    boost::signals2::connection _post_apply_operation_conn;
    boost::signals2::connection _on_irreversible_block_conn;
    boost::signals2::connection _on_remove_comment_cashout_conn;

  public:

    rocksdb_storage_connector( const abstract_plugin& plugin, database& db, const bfs::path& path );
    ~rocksdb_storage_connector() override;
};

}}

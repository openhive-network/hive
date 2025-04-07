#pragma once

#include <hive/chain/external_storage/external_storage_processor.hpp>

#include<hive/chain/database.hpp>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

class rocksdb_storage_connector: public external_storage_snapshot
{
  private:

    database& db;

    external_storage_processor::ptr processor;

    void on_post_apply_operation( const operation_notification& note );
    void on_irreversible_block( uint32_t block_num );
    void on_remove_comment_cashout( const remove_comment_cashout_notification& note );

    boost::signals2::connection _post_apply_operation_conn;
    boost::signals2::connection _on_irreversible_block_conn;
    boost::signals2::connection _on_remove_comment_cashout_conn;

  public:

    rocksdb_storage_connector( const abstract_plugin& plugin, database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    ~rocksdb_storage_connector();

    void supplement_snapshot( const hive::chain::prepare_snapshot_supplement_notification& note ) override;
    void load_additional_data_from_snapshot( const hive::chain::load_snapshot_supplement_notification& note ) override;
};

}}

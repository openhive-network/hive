#include <hive/chain/external_storage/rocksdb_storage_connector.hpp>

#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_storage_processor.hpp>

namespace hive { namespace chain {

rocksdb_storage_connector::rocksdb_storage_connector( const abstract_plugin& plugin, database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
                          : db( db ), processor( std::make_shared<rocksdb_storage_processor>( plugin, db, blockchain_storage_path, storage_path, app ) )
{
  try
  {
    db.set_comments_handler( processor );

    _on_irreversible_block_conn = db.add_irreversible_block_handler(
      [&]( uint32_t block_num )
      {
        on_irreversible_block( block_num );
      }, plugin
    );

    _on_remove_comment_cashout_conn = db.add_remove_comment_cashout_handler(
      [&]( const remove_comment_cashout_notification& note )
      {
        on_remove_comment_cashout( note );
      }, plugin
    );
  }
  FC_CAPTURE_AND_RETHROW()

}

rocksdb_storage_connector::~rocksdb_storage_connector()
{
  chain::util::disconnect_signal( _on_irreversible_block_conn );
  chain::util::disconnect_signal( _on_remove_comment_cashout_conn );
}

void rocksdb_storage_connector::on_irreversible_block( uint32_t block_num )
{
  FC_ASSERT( processor );
  processor->move_to_external_storage( block_num );
}

void rocksdb_storage_connector::on_remove_comment_cashout( const remove_comment_cashout_notification& note )
{
  FC_ASSERT( processor );
  processor->allow_move_to_external_storage( note.comment_id, note.account_id, note.permlink );
}

void rocksdb_storage_connector::supplement_snapshot( const hive::chain::prepare_snapshot_supplement_notification& note )
{
  processor->supplement_snapshot( note );
}

void rocksdb_storage_connector::load_additional_data_from_snapshot( const hive::chain::load_snapshot_supplement_notification& note )
{
  processor->load_additional_data_from_snapshot( note );
}

}}

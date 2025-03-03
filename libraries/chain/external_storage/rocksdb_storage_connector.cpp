#include <hive/chain/external_storage/rocksdb_storage_connector.hpp>

#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_storage_processor.hpp>

namespace hive { namespace chain {

struct post_operation_visitor
{
  external_storage_processor::ptr& processor;

  post_operation_visitor( external_storage_processor::ptr& processor ) : processor( processor ) {}

  typedef void result_type;

  template< typename T >
  void operator()( const T& )const {}

  void operator()( const delete_comment_operation& op )const
  {
    FC_ASSERT( processor );
    processor->delete_comment( op.author, op.permlink );
  }

  void operator()( const comment_operation& op )const
  {
    FC_ASSERT( processor );
    processor->store_comment( op.author, op.permlink );
  }

};

rocksdb_storage_connector::rocksdb_storage_connector( const abstract_plugin& plugin, database& db, const bfs::path& path )
                          : external_storage_connector( std::make_shared<rocksdb_storage_processor>( db, std::make_shared<rocksdb_storage_provider>( path, false ) ) ),
                            db( db )
{
  try
  {
    db.set_external_storage_finder( processor );

    _post_apply_operation_conn = db.add_post_apply_operation_handler(
      [&]( const operation_notification& note )
      {
        on_post_apply_operation( note );
      }, plugin );

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
  chain::util::disconnect_signal( _post_apply_operation_conn );
  chain::util::disconnect_signal( _on_irreversible_block_conn );
  chain::util::disconnect_signal( _on_remove_comment_cashout_conn );
}

void rocksdb_storage_connector::on_post_apply_operation( const operation_notification& note )
{
  note.op.visit( post_operation_visitor( processor ) );
}

void rocksdb_storage_connector::on_irreversible_block( uint32_t block_num )
{
  FC_ASSERT( processor );
  processor->move_to_external_storage( block_num );
}

void rocksdb_storage_connector::on_remove_comment_cashout( const remove_comment_cashout_notification& note )
{
  FC_ASSERT( processor );
  processor->move_to_volatile_storage( note.account_id, note.permlink );
}

}}

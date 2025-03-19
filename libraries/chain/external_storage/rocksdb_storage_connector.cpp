#include <hive/chain/external_storage/rocksdb_storage_connector.hpp>

#include <hive/chain/external_storage/rocksdb_storage_mgr.hpp>
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>

namespace hive { namespace chain {

struct post_operation_visitor
{
  database& db;

  post_operation_visitor( database& db ) : db( db ) {}

  typedef void result_type;

  template< typename T >
  void operator()( const T& )const {}

  void operator()( const comment_operation& op )const
  {
    FC_ASSERT( db.get_external_storage_provider() );

    db.get_external_storage_provider()->store_comment( op );
  }

};

rocksdb_storage_connector::rocksdb_storage_connector( const abstract_plugin& plugin, database& db, const bfs::path& path )
                          : external_storage_connector( db )
{
  ilog( "Initializing external storage manager" );
  db.set_external_storage_provider( std::make_shared<rocksdb_storage_provider>( std::make_shared<rocksdb_storage_mgr>( path, false, db ) ) );
  ilog( "External storage manager has been initialized" );

  try
  {
    _post_apply_operation_conn = db.add_post_apply_operation_handler( [&]( const operation_notification& note ){ on_post_apply_operation( note ); }, plugin, 0 );

    _on_irreversible_block_conn = db.add_irreversible_block_handler(
      [&]( uint32_t block_num )
      {
        on_irreversible_block( block_num );
      },
      plugin
    );
  }
  FC_CAPTURE_AND_RETHROW()

}

rocksdb_storage_connector::~rocksdb_storage_connector()
{
  chain::util::disconnect_signal( _post_apply_operation_conn );
  chain::util::disconnect_signal( _on_irreversible_block_conn );
}

void rocksdb_storage_connector::on_post_apply_operation( const operation_notification& note )
{
  note.op.visit( post_operation_visitor( db ) );
}

void rocksdb_storage_connector::on_irreversible_block( uint32_t block_num )
{
  FC_ASSERT( db.get_external_storage_provider() );

  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_block >();

  auto _it = _volatile_idx.find( block_num );

  while( _it != _volatile_idx.end() && _it->block_number <= block_num )
  {
    if( _it->was_paid )
    {
      db.get_external_storage_provider()->move_to_external_storage( *_it );
    }

    //temporary disabled!!!!
    //const auto& _comment = db.get_comment( _it->comment_id );
    //db.remove( _comment );

    ++_it;
  }
}

}}

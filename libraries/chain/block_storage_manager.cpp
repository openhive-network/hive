#include <hive/chain/block_storage_manager.hpp>

#include <hive/chain/block_log_manager.hpp>
#include <hive/chain/irreversible_block_writer.hpp>
#include <hive/chain/sync_block_writer.hpp>

namespace hive { namespace chain {

block_storage_manager_t::block_storage_manager_t( blockchain_worker_thread_pool& thread_pool,
  database& db, appbase::application& app ) :
  _thread_pool( thread_pool ),
  _app( app ),
  _db( db )
{}

block_write_chain_i* block_storage_manager_t::get_block_writer()
{
  FC_ASSERT( _current_block_writer, "Internal error: block writer not initialized!" );
  return _current_block_writer.get();
}

block_write_chain_i* block_storage_manager_t::init_storage( int block_log_split )
{
  _log_writer = block_log_manager_t::create_writer( block_log_split, _app, _thread_pool );
  _current_block_writer = 
    std::make_unique< sync_block_writer >( *( _log_writer.get() ), _fork_db, _db, _app );

  return _current_block_writer.get();
}

std::shared_ptr< block_write_i > block_storage_manager_t::get_reindex_block_writer()
{
  return std::make_shared< irreversible_block_writer >( *( _log_writer.get() ) );
}

void block_storage_manager_t::open_storage( const block_log_open_args& bl_open_args )
{
  _db.with_write_lock([&]()
  {
    _log_writer->open_and_init( bl_open_args );
  });

  // Get fork db in sync with block log.
  auto head = _log_writer->head_block();
  if( head )
    _fork_db.start_block( head );
}

void block_storage_manager_t::close_storage()
{
  _fork_db.reset();
  _log_writer->close_log();
}

void block_storage_manager_t::on_reindex_start()
{
  _fork_db.reset(); // override effect of fork_db.start_block() call in open()
}

void block_storage_manager_t::on_reindex_end( const std::shared_ptr<full_block_type>& end_block )
{
  _fork_db.start_block( end_block );
}

} } //hive::chain
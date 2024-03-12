#include <hive/chain/block_storage_manager.hpp>

#include <hive/chain/single_file_block_log_writer.hpp>
#include <hive/chain/irreversible_block_writer.hpp>
#include <hive/chain/sync_block_writer.hpp>

namespace hive { namespace chain {

block_storage_manager_t::block_storage_manager_t( database& db, appbase::application& app ) :
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
  // Allowed values are -1, 0 and positive.
  if( block_log_split < LEGACY_SINGLE_FILE_BLOCK_LOG )
  {
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Not supported block log split value" );
  }

  _block_log_split = block_log_split;

  switch( _block_log_split )
  {
    case LEGACY_SINGLE_FILE_BLOCK_LOG:
      {
      _log_writer =
        std::make_unique< single_file_block_log_writer >( _app );
      _current_block_writer = 
        std::make_unique< sync_block_writer >( *( _log_writer.get() ), _fork_db, _db, _app );
      }
      break;
    case MULTIPLE_FILES_FULL_BLOCK_LOG:
       FC_THROW_EXCEPTION( fc::parse_error_exception, "Not implemented block log split value" );
      break;
    default: 
      {
        if( _block_log_split > MULTIPLE_FILES_FULL_BLOCK_LOG )
          FC_THROW_EXCEPTION( fc::parse_error_exception, "Not implemented block log split value" );
        else
          FC_THROW_EXCEPTION( fc::parse_error_exception, "Not supported block log split value" );
      }
      break;
  }

  return _current_block_writer.get();
}

std::shared_ptr< block_write_i > block_storage_manager_t::get_reindex_block_writer()
{
  FC_ASSERT( _block_log_split == LEGACY_SINGLE_FILE_BLOCK_LOG, "Not implemented block log split value" );
  return std::make_shared< irreversible_block_writer >( *( _log_writer.get() ) );
}

void block_storage_manager_t::open_storage( 
  const block_log_open_args& bl_open_args,
  hive::chain::blockchain_worker_thread_pool& thread_pool )
{
  FC_ASSERT( _block_log_split == LEGACY_SINGLE_FILE_BLOCK_LOG, "Not implemented block log split value" );

  _db.with_write_lock([&]()
  {
    _log_writer->open_and_init( bl_open_args, thread_pool );
  });

  // Get fork db in sync with block log.
  auto head = _log_writer->head_block();
  if( head )
    _fork_db.start_block( head );
}

void block_storage_manager_t::close_storage()
{
  _fork_db.reset();
  _log_writer->close();
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
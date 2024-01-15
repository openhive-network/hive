#include <hive/chain/block_storage_manager.hpp>

#include <hive/chain/irreversible_block_writer.hpp>
#include <hive/chain/pruned_block_writer.hpp>

namespace hive { namespace chain {

class no_target_block_storage_t : public target_block_storage_i
{
public:
  virtual ~no_target_block_storage_t() {}
  virtual void store_irreversible_block( const std::shared_ptr<full_block_type>& full_block ) override {}
};

class pruned_target_block_storage_t : public target_block_storage_i
{
public:
  pruned_target_block_storage_t( pruned_block_writer* writer ) : _writer( writer ) {}
  virtual ~pruned_target_block_storage_t() {}
  virtual void store_irreversible_block( const std::shared_ptr<full_block_type>& full_block ) override
  {
    _writer->store_full_block( full_block );
  }

private:
  pruned_block_writer* _writer = nullptr;
};

block_storage_manager_t::block_storage_manager_t( database& db, appbase::application& app ) :
  _app( app ),
  _db( db ),
  _block_log( app )
{}

block_write_chain_i* block_storage_manager_t::get_block_writer()
{
  FC_ASSERT( _current_block_writer, "Internal error: block writer not initialized!" );
  return _current_block_writer.get();
}

block_write_chain_i* block_storage_manager_t::init_storage(
  const std::string& block_storage_type, 
  const std::string& aux_block_storage_type )
{
  /*if( not block_storage_type.empty() )
  {
    if( block_storage_type == "BLOCK_LOG" )
    {
      _storage_type = block_storage_t::BLOCK_LOG;
    }
    else if( block_storage_type == "PRUNED" )
    {*/
      _storage_type = block_storage_t::PRUNED;
    /*}
    else
      FC_THROW_EXCEPTION( fc::parse_error_exception, "Not supported block storage type" );
  }*/

  /*if( not aux_block_storage_type.empty() )
  {
    if( aux_block_storage_type == "BLOCK_LOG" )
    {*/
      _aux_storage_type = block_storage_t::BLOCK_LOG;
    /*}
    else
      FC_THROW_EXCEPTION( fc::parse_error_exception, "Not supported auxiliary block storage type" );
  }*/

  switch( _storage_type )
  {
    case block_storage_t::BLOCK_LOG:
      {
      _current_block_writer = 
        std::make_unique< sync_block_writer >( _block_log, _fork_db, _db, _app );
      /*sync_block_writer* aux =
        new sync_block_writer( _block_log, _fork_db, _db, _app );
      aux->set_aux( new pruned_block_writer( 1024, _db, _app, _fork_db ) );
      _current_block_writer = 
        std::unique_ptr< sync_block_writer >( aux );*/
      }
      break;
    case block_storage_t::PRUNED:
      _current_block_writer = 
        std::make_unique< pruned_block_writer >( 1024, _db, _app, _fork_db );
      break;
    default: FC_THROW_EXCEPTION( fc::parse_error_exception, "Unsupported block storage value set" );
      break;
  }

  return _current_block_writer.get();
}

std::shared_ptr< irreversible_block_writer > block_storage_manager_t::get_reindex_block_writer()
{
  if( _storage_type == block_storage_t::BLOCK_LOG ||
      ( _aux_storage_type && *_aux_storage_type == block_storage_t::BLOCK_LOG ) )
  {
    return std::make_shared< irreversible_block_writer >( _block_log );
  }

  ilog("No auxiliary storage provided for replaying");
  return std::shared_ptr< irreversible_block_writer >();
}

target_block_storage_i& block_storage_manager_t::get_target_block_storage()
{
  if( not _target_block_storage )
  {
    switch( _storage_type )
    {
      case block_storage_t::BLOCK_LOG:
        _target_block_storage = std::make_unique< no_target_block_storage_t >();
        break;
      case block_storage_t::PRUNED:
        {
          pruned_block_writer* writer = dynamic_cast< pruned_block_writer* >( _current_block_writer.get() );
          FC_ASSERT( writer, "Current block writer is not pruned one though the flag says so!" );
          _target_block_storage = std::make_unique< pruned_target_block_storage_t >( writer );
        }
        break;
      default: FC_THROW_EXCEPTION( fc::parse_error_exception, "Unsupported block storage value set" );
        break;
    }
  }
  
  return *_target_block_storage;
}

void block_storage_manager_t::on_snapshot_loaded_without_replay()
{
  auto lib = _current_block_writer->get_irreversible_block_reader().irreversible_head_block();
  _current_block_writer->on_reindex_end( lib );
}

void block_storage_manager_t::open_storage( 
  const hive::chain::open_args& db_open_args,
  hive::chain::blockchain_worker_thread_pool& thread_pool )
{
  if( _storage_type == block_storage_t::BLOCK_LOG || 
      ( _aux_storage_type && *_aux_storage_type == block_storage_t::BLOCK_LOG ) )
  {
    _db.with_write_lock([&]()
    {
      _block_log.open_and_init( db_open_args.data_dir / "block_log",
                                db_open_args.enable_block_log_compression,
                                db_open_args.block_log_compression_level,
                                db_open_args.enable_block_log_auto_fixing,
                                thread_pool );
    });
  }
}

void block_storage_manager_t::close_storage()
{
  _fork_db.reset();
  _block_log.close();
}

} } //hive::chain

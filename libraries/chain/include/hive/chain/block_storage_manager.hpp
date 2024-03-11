#pragma once

#include <hive/chain/block_log_writer_common.hpp>
#include <hive/chain/block_write_chain_interface.hpp>
#include <hive/chain/fork_database.hpp>

#include <memory>

#define LEGACY_SINGLE_FILE_BLOCK_LOG -1
#define MULTIPLE_FILES_FULL_BLOCK_LOG 0

namespace hive { namespace chain {

class block_write_chain_i;
class blockchain_worker_thread_pool;
class database;
struct open_args;

class block_storage_manager_t final
{
public:
  block_storage_manager_t( database& db, appbase::application& app );

  block_write_chain_i* get_block_writer();

  block_write_chain_i* init_storage( int block_log_split );

  std::shared_ptr< block_write_i > get_reindex_block_writer();
  void on_reindex_start();
  void on_reindex_end( const std::shared_ptr<full_block_type>& end_block );

  void open_storage( const hive::chain::open_args& db_open_args,
             hive::chain::blockchain_worker_thread_pool& thread_pool );

  void close_storage();

private:
  int                                         _block_log_split = -1;
  appbase::application&                       _app;
  database&                                   _db;
  std::unique_ptr< block_log_writer_common >  _log_writer;
  fork_database                               _fork_db;
  std::unique_ptr< block_write_chain_i >      _current_block_writer;
};

} }
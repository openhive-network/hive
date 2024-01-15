#pragma once

#include <hive/chain/sync_block_writer.hpp>

#include <appbase/application.hpp>

namespace hive { namespace chain {

//class block_log;
class block_write_chain_i;
class blockchain_worker_thread_pool;
class database;
//class fork_database;
class full_block_type;
class irreversible_block_writer;
struct open_args;
using appbase::application;

class target_block_storage_i
{
public:
  virtual ~target_block_storage_i() {}
  virtual void store_irreversible_block( const std::shared_ptr<full_block_type>& full_block ) = 0;
};

class block_storage_manager_t final
{
public:
  block_storage_manager_t( database& db, appbase::application& app );

  block_write_chain_i* get_block_writer();

  block_write_chain_i* init_storage( const std::string& block_storage_type, 
                                     const std::string& aux_block_storage_type );

  std::shared_ptr< irreversible_block_writer > get_reindex_block_writer();

  target_block_storage_i& get_target_block_storage();

  void open_storage( const hive::chain::open_args& db_open_args,
             hive::chain::blockchain_worker_thread_pool& thread_pool );

  void close_storage();

private:
  enum class block_storage_t
  {
    BLOCK_LOG, // single file (default)
    PRUNED // a pool of recent blocks, kept in memory (state)
  };
  block_storage_t                           _storage_type = block_storage_t::BLOCK_LOG;
  std::optional< block_storage_t >          _aux_storage_type;
  appbase::application&                     _app;
  database&                                 _db;
  block_log                                 _block_log;
  fork_database                             _fork_db;
  std::unique_ptr< block_write_chain_i >    _current_block_writer;
  std::unique_ptr< target_block_storage_i > _target_block_storage;
};

} }

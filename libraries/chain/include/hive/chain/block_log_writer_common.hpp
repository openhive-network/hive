#pragma once

#include <hive/chain/block_log_reader_common.hpp>

namespace hive { namespace chain {

  class block_log;
  class blockchain_worker_thread_pool;
  struct open_args;

  /**
   * Abstract class containing common part of all block-log-based implementations
   * of block writing functionality.
   */
  class block_log_writer_common : public block_log_reader_common
  {
  protected:
    /// To be implemented by subclasses.
    virtual block_log& get_head_block_log() = 0;
    /// To be implemented by subclasses.
    virtual block_log& get_log_for_new_block() = 0;

  public:
    uint64_t append(const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync);
    void flush();

    void open_and_init( const open_args& db_open_args, blockchain_worker_thread_pool& thread_pool );
    void close();
  };

} }

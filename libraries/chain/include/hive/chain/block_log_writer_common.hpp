#pragma once

#include <hive/chain/block_log_reader_common.hpp>

namespace hive { namespace chain {

  class blockchain_worker_thread_pool;
  struct open_args;

  /**
   * Abstract class containing common part of all block-log-based implementations
   * of block writing functionality.
   */
  class block_log_writer_common : public block_log_reader_common
  {
  public:
    struct block_log_open_args
    {
      fc::path  data_dir;
      bool      enable_block_log_compression = true;
      int       block_log_compression_level = 15;
      bool      enable_block_log_auto_fixing = true;
    };
    /// To be implemented by subclasses.
    virtual void open_and_init( const block_log_open_args& bl_open_args ) = 0;
    /// To be implemented by subclasses.
    virtual void open_and_init( const fc::path& path, bool read_only ) = 0;
    /// To be implemented by subclasses.
    virtual void append( const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync ) = 0;
    /// Additional low-level access method to be implemented by subclasses.
    virtual uint64_t append_raw( uint32_t block_num, const char* raw_block_data, size_t raw_block_size,
                                 const block_attributes_t& flags, const bool is_at_live_sync ) = 0;
    /// To be implemented by subclasses.
    virtual void flush_head_log() = 0;
  };

} }

#pragma once

#include <hive/chain/block_log_artifacts.hpp>
#include <hive/chain/block_read_interface.hpp>
#include <hive/chain/detail/block_attributes.hpp>

#define LEGACY_SINGLE_FILE_BLOCK_LOG -1
// There are 4 digits available in split block log file name
#define MAX_FILES_OF_SPLIT_BLOCK_LOG 9999

namespace hive { namespace chain {
  class database;

  /**
   * @brief Represents both block log and memory-only implementations of irreversible block storage.
   * Serves dual purpose as block reader implementation.
   * Use for reading & writing instead of block_log class to make your life easier.
   * Use as block reader based on block log, e.g. while replaying.
   */
  class block_storage_i : public block_read_i
  {
  public:
    using block_storage_ptr_t = std::shared_ptr< block_storage_i >;

    static block_storage_ptr_t create_storage( int block_log_split,
      appbase::application& app, blockchain_worker_thread_pool& thread_pool );
    /// Requires that path points to first path file or legacy single file (no pruned logs accepted).
    /*static block_storage_ptr_t create_opened_wrapper( const fc::path& the_path,
      appbase::application& app, blockchain_worker_thread_pool& thread_pool,
      bool read_only );
    static block_storage_ptr_t create_limited_wrapper( const fc::path& dir,
      appbase::application& app, blockchain_worker_thread_pool& thread_pool,
      uint32_t start_from_part = 1 );*/

  public:
    virtual ~block_storage_i() = default;

    using full_block_ptr_t = std::shared_ptr<full_block_type>;
    using full_block_range_t = std::vector<std::shared_ptr<full_block_type>>;

    // Additional (to block_read_i) methods needed mostly by chain plugin.
    struct block_log_open_args
    {
      fc::path  data_dir;
      bool      enable_block_log_compression = true;
      int       block_log_compression_level = 15;
      bool      enable_block_log_auto_fixing = true;
      bool      load_snapshot = false;
    };
    virtual void open_and_init( const block_log_open_args& bl_open_args, bool read_only,
                                database* lib_access ) = 0;
    virtual void reopen_for_writing() = 0;
    virtual void close_storage() = 0;

    virtual void append( const full_block_ptr_t& full_block, const bool is_at_live_sync ) = 0;
    virtual void flush_head_storage() = 0;

    /** @brief Deletes block log file(s). Use carefully, getting them back takes time.
     *  @param dir Where the file(s) should be deleted.
     *  Note that only files matching block_log_split configuration will be removed.
     */
    virtual void wipe_storage_files( const fc::path& dir ) = 0;
  };

} }

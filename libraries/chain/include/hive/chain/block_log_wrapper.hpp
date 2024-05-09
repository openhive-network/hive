#pragma once

#include <hive/chain/block_log_artifacts.hpp>
#include <hive/chain/block_read_interface.hpp>
#include <hive/chain/detail/block_attributes.hpp>

#define LEGACY_SINGLE_FILE_BLOCK_LOG -1
#define MULTIPLE_FILES_FULL_BLOCK_LOG 0

namespace hive { namespace chain {

  /**
   * @brief Dual purpose block log wrapper / block reader implementation.
   * Use for reading & writing instead of block_log class to make your life easier.
   * Use as block reader based on block log, e.g. while replaying.
   */
  class block_log_wrapper : public block_read_i
  {
  public:
    using block_log_wrapper_t = std::shared_ptr< block_log_wrapper >;

    static block_log_wrapper_t create_wrapper( int block_log_split,
      appbase::application& app, blockchain_worker_thread_pool& thread_pool );
    /// Requires that path points to first path file or legacy single file (no pruned logs accepted).
    static block_log_wrapper_t create_opened_wrapper( const fc::path& input_path,
      appbase::application& app, blockchain_worker_thread_pool& thread_pool,
      bool recreate_artifacts_if_needed = true );

  public:
    block_log_wrapper( int block_log_split, appbase::application& app,
                                 blockchain_worker_thread_pool& thread_pool );
    virtual ~block_log_wrapper() = default;

    using full_block_ptr_t = std::shared_ptr<full_block_type>;
    using full_block_range_t = std::vector<std::shared_ptr<full_block_type>>;

    // Methods wrapping safely block_log ones.
    struct block_log_open_args
    {
      fc::path  data_dir;
      bool      enable_block_log_compression = true;
      int       block_log_compression_level = 15;
      bool      enable_block_log_auto_fixing = true;
    };
    void open_and_init( const block_log_open_args& bl_open_args );
    void open_and_init( const fc::path& path, bool read_only );
    void close_log();
    std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t> read_raw_head_block() const;
    std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t>
      read_raw_block_data_by_num(uint32_t block_num) const;
    void append( const full_block_ptr_t& full_block, const bool is_at_live_sync );
    uint64_t append_raw( uint32_t block_num, const char* raw_block_data, size_t raw_block_size,
                                 const block_attributes_t& flags, const bool is_at_live_sync );
    void flush_head_log();

    // Methods implementing block_read_i interface:
    virtual full_block_ptr_t head_block() const override;
    virtual uint32_t head_block_num( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;
    virtual block_id_type head_block_id( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;
    virtual full_block_ptr_t read_block_by_num( uint32_t block_num ) const override;
    virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor,
                                 blockchain_worker_thread_pool& thread_pool ) const override;
    virtual full_block_ptr_t fetch_block_by_id( const block_id_type& id ) const override;
     virtual full_block_ptr_t get_block_by_number(
      uint32_t block_num, fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;
    virtual full_block_range_t fetch_block_range( const uint32_t starting_block_num, 
      const uint32_t count, fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;
    virtual bool is_known_block( const block_id_type& id ) const override;
    virtual std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(
      const std::deque<block_id_type>& item_hashes_received ) const override;
    virtual block_id_type find_block_id_for_num( uint32_t block_num ) const override;
    virtual std::vector<block_id_type> get_blockchain_synopsis(
      const block_id_type& reference_point, 
      uint32_t number_of_blocks_after_reference_point ) const override;
    virtual std::vector<block_id_type> get_block_ids(
      const std::vector<block_id_type>& blockchain_synopsis,
      uint32_t& remaining_item_count,
      uint32_t limit) const override;

    /** @brief Deletes block log file(s). Use carefully, getting them back takes time.
     *  @param dir Where the file(s) should be deleted.
     *  Note that only files matching block_log_split configuration will be removed.
     */
    void wipe_files( const fc::path& dir );

  private:
    // Common helpers
    void common_open_and_init( std::optional< bool > read_only );
    void internal_open_and_init( block_log* the_log, const fc::path& path, bool read_only );
    uint32_t validate_tail_part_number( uint32_t tail_part_number, uint32_t head_part_number ) const;

    const block_log* get_block_log_corresponding_to( uint32_t block_num ) const;
    const full_block_ptr_t get_head_block() const;
    block_id_type read_block_id_by_num( uint32_t block_num ) const;
    full_block_range_t read_block_range_by_num( uint32_t starting_block_num, uint32_t count ) const;

    using append_t = std::function< void( block_log* log ) >;
    void internal_append( uint32_t block_num, append_t do_appending);

    bool is_last_number_of_the_file( uint32_t block_num ) const
    {
      return block_num > 0 && 
            ( block_num % _max_blocks_in_log_file == 0 );
    }

    /** Returns 1 for 0,
     *  1 for [1 .. _max_blocks_in_log_file] &
     *  N for [1+(N-1)*_max_blocks_in_log_file .. N*_max_blocks_in_log_file]
     */
    uint32_t get_part_number_for_block( uint32_t block_num ) const
    {
      return block_num == 0 ? 1 :
        ( (uint64_t)block_num + (uint64_t)_max_blocks_in_log_file -1 ) / _max_blocks_in_log_file;
    }

    /// Returns the number of oldest stored block.
    uint32_t get_tail_block_num() const;

  private:
    appbase::application&           _app;
    blockchain_worker_thread_pool&  _thread_pool;
    const uint32_t                  _max_blocks_in_log_file = 0;
    const int                       _block_log_split = 0;
    block_log_open_args             _open_args;
    std::deque< block_log* >        _logs;
  };

} }

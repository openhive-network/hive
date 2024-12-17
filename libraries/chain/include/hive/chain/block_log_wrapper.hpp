#pragma once

#include <hive/chain/block_log_artifacts.hpp>
#include <hive/chain/block_storage_interface.hpp>
#include <hive/chain/detail/block_attributes.hpp>

namespace hive { namespace chain {

  class database;

  /**
   * @brief Dual purpose block log wrapper / block storage/reader implementation.
   * Use for reading & writing instead of block_log class to make your life easier.
   */
  class block_log_wrapper : public block_storage_i
  {
  public:
    using block_log_wrapper_t = std::shared_ptr< block_log_wrapper >;

    /// Requires that path points to first path file or legacy single file (no pruned logs accepted).
    static block_log_wrapper_t create_opened_wrapper( const fc::path& the_path,
      appbase::application& app, blockchain_worker_thread_pool& thread_pool,
      bool read_only );
    static block_log_wrapper_t create_limited_wrapper( const fc::path& dir,
      appbase::application& app, blockchain_worker_thread_pool& thread_pool,
      uint32_t start_from_part = 1 );

  public:
    block_log_wrapper( int block_log_split, appbase::application& app,
                                 blockchain_worker_thread_pool& thread_pool );
    virtual ~block_log_wrapper() = default;

    using block_storage_i::full_block_ptr_t;
    using block_storage_i::full_block_range_t;
    using block_storage_i::block_log_open_args;

    // Required by block_storage_i:
    virtual void open_and_init( const block_log_open_args& bl_open_args, bool read_only,
                                database* db ) override;
    virtual void reopen_for_writing() override;
    virtual void close_storage() override;
    virtual void append( const full_block_ptr_t& full_block, const bool is_at_live_sync ) override;
    virtual void flush_head_storage() override;
    virtual void wipe_storage_files( const fc::path& dir ) override;

    // Methods wrapping safely block_log ones.
    std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t> read_raw_head_block() const;
    std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t>
      read_common_raw_block_data_by_num(uint32_t block_num) const;
    void multi_read_raw_block_data(uint32_t first_block_num, uint32_t last_block_num_from_disk,
      block_log_artifacts::artifact_container_t& plural_of_block_artifacts,
      std::unique_ptr<char[]>& block_data_buffer, size_t& block_data_buffer_size ) const;
    uint64_t append_raw( uint32_t block_num, const char* raw_block_data, size_t raw_block_size,
                                 const block_attributes_t& flags, const bool is_at_live_sync );
    void multi_append_raw( uint32_t first_block_num, std::unique_ptr<char[]>& block_data_buffer,
      block_log_artifacts::artifact_container_t& plural_of_artifacts);

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

    /** Returns 1 for 0,
     *  1 for [1 .. _max_blocks_in_log_file] &
     *  N for [1+(N-1)*_max_blocks_in_log_file .. N*_max_blocks_in_log_file]
     */
    static uint32_t get_part_number_for_block( uint32_t block_num, int block_log_split )
    {
      return get_part_number_for_block( 
               block_num,
               determine_max_blocks_in_log_file( block_log_split )
             );
    }

    static uint32_t get_number_of_first_block_in_part( uint32_t part_number, int block_log_split )
    {
      FC_ASSERT( part_number > 0 );
      return 1 + (part_number -1) * determine_max_blocks_in_log_file( block_log_split );
    }

    static uint32_t get_number_of_last_block_in_part( uint32_t part_number, int block_log_split )
    {
      FC_ASSERT( part_number > 0 );
      return part_number * determine_max_blocks_in_log_file( block_log_split );
    }

  private:
    static uint32_t determine_max_blocks_in_log_file( int block_log_split );

    void skip_first_parts( uint32_t parts_to_skip );

    bool try_splitting_monolithic_log_file( full_block_ptr_t state_head_block,
                                            uint32_t head_block_number = 0,
                                            size_t part_count = 0 );
    struct part_file_info_t {
      uint32_t part_number = 0;
      fc::path part_file;

      bool operator<(const part_file_info_t& o) const { return part_number < o.part_number; }
    };
    using part_file_names_t=std::set< part_file_info_t >;
    void look_for_part_files( part_file_names_t& part_file_names );
    /**
     * @brief Makes sure all parts required by configuration are found or generated.
     * 
     * @throws runtime_error when unable to find or generate enough file parts.
     * @param head_part_number file part with biggest number (containing head block).
     * @param actual_tail_needed file part with lowest number required.
     * @param part_file_names contains all parts found in block log directory.
     * @param state_head_block pre-read from state file.
     * @return actual tail part number needed (and present).
     */
    void force_parts_exist( uint32_t head_part_number, uint32_t actual_tail_needed,
      part_file_names_t& part_file_names, bool allow_splitting_monolithic_log,
      full_block_ptr_t state_head_block );
    /// @brief Used internally by create_opened_wrapper
    void open_and_init( const fc::path& path, bool read_only, uint32_t start_from_part = 1 );
    // Common helpers
    void common_open_and_init( bool read_only, bool allow_splitting_monolithic_log,
                               full_block_ptr_t state_head_block, uint32_t start_from_part = 1 );
    using block_log_ptr_t = std::shared_ptr<block_log>;
    void internal_open_and_init( block_log_ptr_t the_log, const fc::path& path, bool read_only );
    uint32_t validate_tail_part_number( uint32_t tail_part_number, uint32_t head_part_number ) const;

    block_log_ptr_t get_head_log() const;
    const block_log_ptr_t get_block_log_corresponding_to( uint32_t block_num ) const;
    const full_block_ptr_t get_head_block() const;
    block_id_type read_block_id_by_num( uint32_t block_num ) const;
    full_block_range_t read_block_range_by_num( uint32_t starting_block_num, uint32_t count ) const;

    using append_t = std::function< void( block_log_ptr_t log ) >;
    void internal_append( uint32_t first_block_num, size_t block_count, append_t do_appending);

    static uint32_t get_part_number_for_block( uint32_t block_num, uint32_t max_blocks_in_log_file )
    {
      return block_num == 0 ? 1 :
        ( (uint64_t)block_num + (uint64_t)max_blocks_in_log_file -1 ) / max_blocks_in_log_file;
      // 0: 1
      // 1: 1 + 1 000 000 -1 / 1 000 000 == 1 000 000 / 1 000 000 == 1
      // 2: 2 + 1 000 000 -1 / 1 000 000 == 1 000 001 / 1 000 000 == 1
      // 999 999: 999 999 + 1 000 000 -1 / 1 000 000 == 1 999 998 / 1 000 000 == 1
      // 1 000 000: 1 000 000 + 1 000 000 -1 / 1 000 000 == 1 999 999 / 1 000 000 == 1
      // 1 000 001: 1 000 001 + 1 000 000 -1 / 1 000 000 == 2 000 000 / 1 000 000 == 2
      // 1 000 002: 1 000 002 + 1 000 000 -1 / 1 000 000 == 2 000 001 / 1 000 000 == 2
    }

    bool is_last_number_of_the_file( uint32_t block_num ) const
    {
      return block_num % _max_blocks_in_log_file == 0;
      // 0: true
      // 1: 1 % 1 000 000 == 1 / false
      // 2: 2 % 1 000 000 == 2 / false
      // 999 999: 999 999 % 1 000 000 == 999 999 / false
      // 1 000 000: 1 000 000 % 1 000 000 == 0 / true
      // 1 000 001: 1 000 001 % 1 000 000 == 1 / false
      // 1 000 002: 1 000 002 % 1 000 000 == 2 / false
    }

    /// Returns the number of oldest stored block.
    uint32_t get_actual_tail_block_num() const;

    void dispose_garbage( bool closing_time );

  private:
    appbase::application&             _app;
    blockchain_worker_thread_pool&    _thread_pool;
    const uint32_t                    _max_blocks_in_log_file = 0;
    const int                         _block_log_split = 0;
    block_log_open_args               _open_args;
    std::deque< block_log_ptr_t >     _logs;
    std::deque< block_log_ptr_t >     _garbage_collection;
  };

} }

#pragma once

#include <hive/chain/block_storage_interface.hpp>

namespace hive { namespace chain {

  /**
   * @brief block storage implementation containing only head block.
   */
  class single_block_storage : public block_storage_i
  {
  public:
    single_block_storage( appbase::application& app, blockchain_worker_thread_pool& thread_pool );
    virtual ~single_block_storage() = default;

    using block_storage_i::full_block_ptr_t;
    using block_storage_i::full_block_range_t;
    using block_storage_i::block_log_open_args;

    // Required by block_storage_i:
    virtual void open_and_init( const block_log_open_args& bl_open_args, bool read_only,
                                database* lib_access ) override;
    virtual void reopen_for_writing() override;
    virtual void close_storage() override;
    virtual void append( const full_block_ptr_t& full_block, const bool is_at_live_sync ) override;
    virtual void flush_head_storage() override;
    virtual void wipe_storage_files( const fc::path& dir ) override;

    // Methods implementing block_read_i interface:
    virtual full_block_ptr_t head_block() const override;
    virtual uint32_t head_block_num( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;
    virtual uint32_t tail_block_num( 
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

  private:
    appbase::application&           _app;
    blockchain_worker_thread_pool&  _thread_pool;
    block_log_open_args             _open_args;
    database*                       _db = nullptr;
  };

} }

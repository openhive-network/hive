#pragma once

#include <hive/chain/block_log_artifacts.hpp>
#include <hive/chain/block_read_interface.hpp>
#include <hive/chain/detail/block_attributes.hpp>

namespace hive { namespace chain {

  /**
   * Abstract class containing common part of all block-log-based implementations
   * of block read interface
   */
  class block_log_reader_common : public block_read_i
  {
  protected:
    /// To be implemented by subclasses.
    virtual const std::shared_ptr<full_block_type> get_head_block() const = 0;

    /// To be implemented by subclasses.
    virtual block_id_type read_block_id_by_num( uint32_t block_num ) const = 0;

    using block_range_t = std::vector<std::shared_ptr<full_block_type>>;
    /// To be implemented by subclasses.
    virtual block_range_t read_block_range_by_num( uint32_t starting_block_num, uint32_t count ) const = 0;

  public:
    /// To be implemented by subclasses.
    virtual void close_log() = 0;

    virtual std::shared_ptr<full_block_type> head_block() const override;

    virtual uint32_t head_block_num( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;
    virtual block_id_type head_block_id( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;

    // The two methods of block_read_i left to be implemented by subclasses:
    //
    //virtual std::shared_ptr<full_block_type> read_block_by_num( uint32_t block_num ) const override;
    //
    //virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
    //                             block_processor_t processor, hive::chain::blockchain_worker_thread_pool& thread_pool ) const override;

    virtual std::shared_ptr<full_block_type> fetch_block_by_number( uint32_t block_num,
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;

    virtual std::shared_ptr<full_block_type> fetch_block_by_id( 
      const block_id_type& id ) const override; 

    virtual bool is_known_block( const block_id_type& id ) const override;

    virtual std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(
      const std::deque<block_id_type>& item_hashes_received ) const override;

    virtual block_id_type find_block_id_for_num( uint32_t block_num ) const override;

    virtual std::vector<std::shared_ptr<full_block_type>> fetch_block_range( 
      const uint32_t starting_block_num, const uint32_t count, 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;

    virtual std::vector<block_id_type> get_blockchain_synopsis(
      const block_id_type& reference_point, 
      uint32_t number_of_blocks_after_reference_point ) const override;

    virtual std::vector<block_id_type> get_block_ids(
      const std::vector<block_id_type>& blockchain_synopsis,
      uint32_t& remaining_item_count,
      uint32_t limit) const override;

    /// Additional low-level access method to be implemented by subclasses.
    using block_attributes_t=hive::chain::detail::block_attributes_t;
    virtual std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t> read_raw_head_block() const = 0;
    /// Additional low-level access method to be implemented by subclasses.
    virtual std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> read_raw_block_data_by_num(uint32_t block_num) const = 0;
  };

} }

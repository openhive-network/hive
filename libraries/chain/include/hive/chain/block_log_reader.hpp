#pragma once

#include <hive/chain/block_read_interface.hpp>

namespace hive { namespace chain {

  class block_log;

  class block_log_reader : public block_read_i
  {
  public:
    block_log_reader( const block_log& the_log );
    virtual ~block_log_reader() = default;

    virtual std::shared_ptr<full_block_type> head_block() const override;

    virtual uint32_t head_block_num( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;
    virtual block_id_type head_block_id( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;

    virtual std::shared_ptr<full_block_type> read_block_by_num( uint32_t block_num ) const override;

    virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor, hive::chain::blockchain_worker_thread_pool& thread_pool ) const override;

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

  private:
    const block_log& _block_log;
  };

} }

#pragma once

#include <hive/chain/block_read_interface.hpp>

namespace hive { namespace chain {

  class database;
  class fork_database;
  class recent_block_i;

  class pruned_block_writer : public block_read_i
  {
  public:
    pruned_block_writer( database& db, const fork_database& fork_db, const recent_block_i& );
    virtual ~pruned_block_writer() = default;

    virtual uint32_t head_block_num( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;
    virtual block_id_type head_block_id( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;

    virtual std::shared_ptr<full_block_type> read_block_by_num( uint32_t block_num ) const override;

    virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor,
                                 hive::chain::blockchain_worker_thread_pool& thread_pool ) const override;

    virtual std::shared_ptr<full_block_type> fetch_block_by_number( uint32_t block_num,
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;

    virtual std::shared_ptr<full_block_type> fetch_block_by_id( 
      const block_id_type& id ) const override;

    virtual bool is_known_block( const block_id_type& id ) const override;

    virtual std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(
      const std::deque<block_id_type>& item_hashes_received ) const override;

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
    void store_full_block( const std::shared_ptr<full_block_type> full_block );
    std::shared_ptr<full_block_type> retrieve_full_block( uint16_t recent_block_num );

  private:
    database& _db;
    const fork_database& _fork_db;
    const recent_block_i& _recent_blocks;
  };

} }

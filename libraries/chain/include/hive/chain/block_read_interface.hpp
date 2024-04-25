#pragma once

#include <hive/protocol/block.hpp>
#include <hive/chain/full_block.hpp>

#include <fc/time.hpp>

#include <deque>
#include <memory>
#include <vector>

namespace hive { namespace chain {

  class blockchain_worker_thread_pool;
  
  class block_read_i
  {
  protected:
    virtual ~block_read_i() = default;

  public:
    virtual std::shared_ptr<full_block_type> head_block() const = 0;
    virtual uint32_t head_block_num( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const = 0;
    virtual block_id_type head_block_id( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const = 0;

    virtual std::shared_ptr<full_block_type> read_block_by_num( uint32_t block_num ) const = 0;

    using block_processor_t = std::function<bool( const std::shared_ptr<full_block_type>& )>;
    /// Let provided processor walk over provided block range.
    virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor, hive::chain::blockchain_worker_thread_pool& thread_pool ) const = 0;

    /// Check among reversible blocks. If found none check among irreversible blocks.
    virtual std::shared_ptr<full_block_type> fetch_block_by_id( const block_id_type& id ) const = 0;
    /** Check among reversible blocks. If found 1 return it, if found more return the one
     *  from main branch. If found none check among irreversible blocks.
    */
    virtual std::shared_ptr<full_block_type> fetch_block_by_number( 
      uint32_t block_num, fc::microseconds wait_for_microseconds = fc::microseconds() ) const = 0;

    /// Check among reversible blocks on main branch. Supplement with irreversible blocks when needed.
    virtual std::vector<std::shared_ptr<full_block_type>> fetch_block_range( 
      const uint32_t starting_block_num, const uint32_t count, 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const = 0;

    /// Check among (linked & unlinked) reversible blocks then among irreversible if needed.
    virtual bool is_known_block(const block_id_type& id) const = 0;
    /** Needed by p2p plugin only. 
     *  Check among (linked only) reversible blocks then among irreversible if needed.
    */
    virtual std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(
      const std::deque<block_id_type>& item_hashes_received ) const = 0;
    /// Check among reversible blocks on main branch then among irreversible.
    virtual block_id_type find_block_id_for_num( uint32_t block_num ) const = 0;
    /** Needed by p2p plugin only.
     *  See implementations of database::get_blockchain_synopsis & fork_database::get_blockchain_synopsis
    */
    virtual std::vector<block_id_type> get_blockchain_synopsis(
      const block_id_type& reference_point,
      uint32_t number_of_blocks_after_reference_point ) const = 0;
    /** used by the p2p layer, get_block_ids takes a blockchain synopsis provided by a peer, and
     *  generates a sequential list of block ids that builds off of the last item in the synopsis
     *  that we have in common
     */
    virtual std::vector<block_id_type> get_block_ids(
      const std::vector<block_id_type>& blockchain_synopsis,
      uint32_t& remaining_item_count,
      uint32_t limit) const = 0;
  };

} }


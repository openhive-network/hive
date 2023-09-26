#pragma once

#include <hive/protocol/block.hpp>
#include <hive/chain/full_block.hpp>

#include <fc/time.hpp>

#include <deque>

namespace hive { namespace chain {

  class block_read_i
  {
  public:
    virtual ~block_read_i() = default;
  
    virtual std::shared_ptr<full_block_type> head() const = 0;

    virtual std::shared_ptr<full_block_type> read_block_by_num( uint32_t block_num ) const = 0;

    using block_processor_t = std::function<bool( const std::shared_ptr<full_block_type>& )>;
    virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor ) = 0;

	/// Check among reversible blocks. If found none check among irreversible blocks.
	//virtual std::shared_ptr<full_block_type> fetch_block_by_id( const block_id_type& id ) const = 0;
	/** Check among reversible blocks. If found 1 return it, if found more return the one
   *  from main branch. If found none check among irreversible blocks.
  */
	/*virtual std::shared_ptr<full_block_type> fetch_block_by_number( 
    uint32_t block_num, fc::microseconds wait_for_microseconds = fc::microseconds() ) const = 0;
	
	/// Check among reversible blocks on main branch. Supplement with irreversible blocks when needed.
	virtual std::vector<std::shared_ptr<full_block_type>> fetch_block_range( 
    const uint32_t starting_block_num, const uint32_t count, 
    fc::microseconds wait_for_microseconds = fc::microseconds() ) = 0;*/

	/// Check among (linked & unlinked) reversible blocks then among irreversible if needed.
	virtual bool is_known_block(const block_id_type& id) const = 0;
	/** Needed by p2p plugin only. 
   *  Check among (linked only) reversible blocks then among irreversible if needed.
  */
	virtual bool is_known_block_unlocked(const block_id_type& id) const = 0;
	/** Needed by p2p plugin only. 
   *  Check among (linked only) reversible blocks then among irreversible if needed.
  */
  virtual std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(
    const std::deque<block_id_type>& item_hashes_received ) const = 0;
	/// Check among reversible blocks on main branch then among irreversible.
	//virtual block_id_type find_block_id_for_num( uint32_t block_num ) const = 0;
	/** Needed by p2p plugin only.
   *  Check among reversible blocks on main branch then among irreversible.
  */
	//virtual bool is_included_block_unlocked(const block_id_type& block_id) = 0;
	/** Needed by p2p plugin only.
   *  See implementations of database::get_blockchain_synopsis & fork_database::get_blockchain_synopsis
  */
	/*virtual std::vector<block_id_type> get_blockchain_synopsis(
    const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point) = 0;
	/// Needed by p2p plugin only.
	virtual std::vector<block_id_type> get_block_ids(
    const std::vector<block_id_type>& blockchain_synopsis,
    uint32_t& remaining_item_count,
    uint32_t limit) = 0;*/
  };

} }


#include <hive/chain/pruned_block_reader.hpp>

//#include <hive/chain/fork_database.hpp>

namespace hive { namespace chain {

pruned_block_reader::pruned_block_reader( const recent_block_i& recent_blocks )
  : _recent_blocks( recent_blocks )
{}

uint32_t pruned_block_reader::head_block_num( 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

block_id_type pruned_block_reader::head_block_id( 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

std::shared_ptr<full_block_type> pruned_block_reader::read_block_by_num( uint32_t block_num ) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

void pruned_block_reader::process_blocks(uint32_t starting_block_number, uint32_t ending_block_number,
  block_processor_t processor, hive::chain::blockchain_worker_thread_pool& thread_pool) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

std::shared_ptr<full_block_type> pruned_block_reader::fetch_block_by_number( uint32_t block_num,
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{ 
  FC_ASSERT(false, "Not implemented yet!");
}

std::shared_ptr<full_block_type> pruned_block_reader::fetch_block_by_id( 
  const block_id_type& id ) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

bool pruned_block_reader::is_known_block(const block_id_type& id) const
{
  std::optional<uint16_t> block_num = _recent_blocks.find_recent_block_num( id );
  return block_num.has_value();
}

std::deque<block_id_type>::const_iterator pruned_block_reader::find_first_item_not_in_blockchain(
  const std::deque<block_id_type>& item_hashes_received ) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

std::vector<std::shared_ptr<full_block_type>> pruned_block_reader::fetch_block_range( 
  const uint32_t starting_block_num, const uint32_t count, 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{ 
  FC_ASSERT(false, "Not implemented yet!");
}

std::vector<block_id_type> pruned_block_reader::get_blockchain_synopsis( 
  const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point ) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

std::vector<block_id_type> pruned_block_reader::get_block_ids(
  const std::vector<block_id_type>& blockchain_synopsis, uint32_t& remaining_item_count,
  uint32_t limit) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

} } //hive::chain


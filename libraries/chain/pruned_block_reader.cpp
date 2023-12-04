#include <hive/chain/pruned_block_reader.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/fork_database.hpp>
#include <hive/chain/full_block_object.hpp>

namespace hive { namespace chain {

void initialize_pruning_indexes( database& db );

pruned_block_reader::pruned_block_reader( database& db,
  const fork_database& fork_db, const recent_block_i& recent_blocks )
  : _db( db ), _fork_db( fork_db ), _recent_blocks( recent_blocks )
{
  initialize_pruning_indexes( _db );
}

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

void pruned_block_reader::store_full_block( const std::shared_ptr<full_block_type> full_block )
{
  try
  {
    const compressed_block_data& cbd = full_block->get_compressed_block();

    const block_attributes_t& attributes = cbd.compression_attributes;
    size_t byte_size = cbd.compressed_size;
    const char* bytes = cbd.compressed_bytes.get();

    const block_id_type& block_id = full_block->get_block_id();

    _db.create< full_block_object >( attributes, byte_size, bytes, block_id );
  }
  FC_CAPTURE_AND_RETHROW()
}

std::shared_ptr<full_block_type> pruned_block_reader::retrieve_full_block( uint16_t recent_block_num )
{
  try
  {
    full_block_object::id_type bsid( recent_block_num );
    const full_block_object* fbo = _db.find<full_block_object, by_id>( bsid );
    if( fbo == nullptr )
      return std::shared_ptr<full_block_type>();

    size_t raw_block_size = fbo->byte_size;
    std::unique_ptr<char[]> compressed_block_data(new char[raw_block_size]);
    memcpy(compressed_block_data.get(), fbo->block_bytes.data(), raw_block_size);
    const block_attributes_t& attributes = fbo->compression_attributes;
    std::optional< block_id_type > block_id( fbo->block_id );

    return full_block_type::create_from_compressed_block_data(
      std::move( compressed_block_data ),
      raw_block_size,
      attributes,
      block_id
    );
  }
  FC_CAPTURE_AND_RETHROW()
}

} } //hive::chain


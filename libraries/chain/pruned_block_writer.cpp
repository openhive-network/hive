#include <hive/chain/pruned_block_writer.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/fork_database.hpp>
#include <hive/chain/full_block_object.hpp>

namespace hive { namespace chain {

void initialize_pruning_indexes( database& db );

pruned_block_writer::pruned_block_writer( uint16_t stored_block_number, 
  database& db, const fork_database& fork_db, const recent_block_i& recent_blocks )
  : _stored_block_number(stored_block_number), _db( db ), _fork_db( fork_db ),
    _recent_blocks( recent_blocks )
{
  FC_ASSERT( stored_block_number > 0, "At least one full block must be stored!" );

  initialize_pruning_indexes( _db );

  for( uint16_t i = 0; i < stored_block_number; ++i )
    _db.create< full_block_object >();
}

uint32_t pruned_block_writer::head_block_num( 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

block_id_type pruned_block_writer::head_block_id( 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

std::shared_ptr<full_block_type> pruned_block_writer::read_block_by_num( uint32_t block_num ) const
{
  return retrieve_full_block( block_num );
}

void pruned_block_writer::process_blocks(uint32_t starting_block_number, uint32_t ending_block_number,
  block_processor_t processor, hive::chain::blockchain_worker_thread_pool& thread_pool) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

std::shared_ptr<full_block_type> pruned_block_writer::fetch_block_by_number( uint32_t block_num,
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{ 
  try {
    shared_ptr<fork_item> forkdb_item = 
      _fork_db.fetch_block_on_main_branch_by_number(block_num, wait_for_microseconds);
    if (forkdb_item)
      return forkdb_item->full_block;

    return read_block_by_num(block_num);
  } FC_LOG_AND_RETHROW()
}

std::shared_ptr<full_block_type> pruned_block_writer::fetch_block_by_id( 
  const block_id_type& id ) const
{
  try {
    shared_ptr<fork_item> fork_item = _fork_db.fetch_block( id );
    if (fork_item)
      return fork_item->full_block;

    std::shared_ptr<full_block_type> block = 
      read_block_by_num( protocol::block_header::num_from_id( id ) );
    if( block && block->get_block_id() == id )
      return block;
      
    return std::shared_ptr<full_block_type>();
  } FC_CAPTURE_AND_RETHROW()
}

bool pruned_block_writer::is_known_block(const block_id_type& id) const
{
  try {
    // Check among reversible blocks in fork db.
    if( _fork_db.fetch_block( id ) )
      return true;

    // Then check among recent blocks in state.
    std::optional<uint16_t> block_num = _recent_blocks.find_recent_block_num( id );
    return block_num.has_value();
  } FC_CAPTURE_AND_RETHROW()
}

std::deque<block_id_type>::const_iterator pruned_block_writer::find_first_item_not_in_blockchain(
  const std::deque<block_id_type>& item_hashes_received ) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

std::vector<std::shared_ptr<full_block_type>> pruned_block_writer::fetch_block_range( 
  const uint32_t starting_block_num, const uint32_t count, 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{ 
  FC_ASSERT(false, "Not implemented yet!");
}

std::vector<block_id_type> pruned_block_writer::get_blockchain_synopsis( 
  const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point ) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

std::vector<block_id_type> pruned_block_writer::get_block_ids(
  const std::vector<block_id_type>& blockchain_synopsis, uint32_t& remaining_item_count,
  uint32_t limit) const
{
  FC_ASSERT(false, "Not implemented yet!");
}

void pruned_block_writer::store_full_block( const std::shared_ptr<full_block_type> full_block )
{
  try
  {
    full_block_object::id_type fbid( full_block->get_block_num() % _stored_block_number );
    _db.modify( _db.get< full_block_object >( fbid ), [&](full_block_object& fbo) {
      const compressed_block_data& cbd = full_block->get_compressed_block();
      size_t byte_size = cbd.compressed_size;
      const char* bytes = cbd.compressed_bytes.get();

      fbo.compression_attributes = cbd.compression_attributes;
      fbo.byte_size = byte_size;
      fbo.block_bytes.assign( bytes, bytes+byte_size );
      fbo.block_id = full_block->get_block_id();
    });
  }
  FC_CAPTURE_AND_RETHROW()
}

std::shared_ptr<full_block_type> pruned_block_writer::retrieve_full_block( uint32_t recent_block_num ) const
{
  try
  {
    full_block_object::id_type fbid( recent_block_num % _stored_block_number );
    const full_block_object* fbo = _db.find<full_block_object, by_id>( fbid );
    if( fbo == nullptr )
      return std::shared_ptr<full_block_type>();

    uint32_t actual_block_num = block_header::num_from_id( fbo->block_id );
    if( actual_block_num != recent_block_num )
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


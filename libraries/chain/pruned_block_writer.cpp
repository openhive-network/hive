#include <hive/chain/pruned_block_writer.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/fork_database.hpp>
#include <hive/chain/fork_db_block_reader.hpp>
#include <hive/chain/full_block_object.hpp>
#include <hive/chain/sync_block_writer.hpp>

namespace hive { namespace chain {

void initialize_pruning_indexes( database& db );

pruned_block_writer::pruned_block_writer( uint16_t stored_block_number, 
  database& db, application& app, fork_database& fork_db )
  : _stored_block_number(stored_block_number), _fork_db( fork_db ), _db( db ), _app( app )
{
  FC_ASSERT( stored_block_number > 0, "At least one full block must be stored!" );

  initialize_pruning_indexes( _db );
}

void pruned_block_writer::on_reindex_start()
{
  _fork_db.reset(); // override effect of fork_db.start_block() call in open()
}

void pruned_block_writer::on_reindex_end( const std::shared_ptr<full_block_type>& end_block )
{
  _fork_db.start_block( end_block );
}

bool pruned_block_writer::push_block(const std::shared_ptr<full_block_type>& full_block, 
  const block_flow_control& block_ctrl,
  uint32_t state_head_block_num,
  block_id_type state_head_block_id,
  const uint32_t skip,
  apply_block_t apply_block_extended,
  pop_block_t pop_block_extended )
{
  return sync_block_writer::push_block( _fork_db, _db, _app, full_block, block_ctrl,
    state_head_block_num, state_head_block_id, skip, apply_block_extended, pop_block_extended );
}

void pruned_block_writer::switch_forks( const block_id_type& new_head_block_id, uint32_t new_head_block_num,
  uint32_t skip, const block_flow_control* pushed_block_ctrl,
  const block_id_type original_head_block_id, const uint32_t original_head_block_number,
  apply_block_t apply_block_extended, pop_block_t pop_block_extended )
{
  sync_block_writer::switch_forks( _fork_db, _db, new_head_block_id, new_head_block_num, skip,
    pushed_block_ctrl, original_head_block_id, original_head_block_number, apply_block_extended,
    pop_block_extended );
}

void pruned_block_writer::initialize_block_data()
{
  // Create irreversible data object pool if needed.
  const auto& idx = _db.get_index< full_block_index, by_num >();
  if( idx.begin() == idx.end() )
  {
    for( uint16_t i = 0; i < _stored_block_number; ++i )
      _db.create< full_block_object >();
    // TODO fill the pool with data provided by other writer via chain plugin, if available.
  }

  // Get fork db in sync with full block objects.
  const full_block_object& head_block = *( idx.rbegin() );
  if( head_block.get_num() )
    _fork_db.start_block( head_block.create_full_block() );
}

void pruned_block_writer::store_block( uint32_t current_irreversible_block_num,
  uint32_t state_head_block_number )
{
  const full_block_object& head_block = get_head_block_data();
  uint32_t irreversible_head_num = head_block.get_num();

  return sync_block_writer::store_block(
    _fork_db,
    current_irreversible_block_num,
    state_head_block_number,
    irreversible_head_num,
    [&]( const std::shared_ptr<full_block_type>& full_block ) { store_full_block( full_block ); },
    [&](){} );
}

void pruned_block_writer::pop_block()
{
  _fork_db.pop_block();
}

std::optional<block_write_i::new_last_irreversible_block_t> 
pruned_block_writer::find_new_last_irreversible_block(
  const std::vector<const witness_object*>& scheduled_witness_objects,
  const std::map<account_name_type, block_id_type>& last_fast_approved_block_by_witness,
  const unsigned witnesses_required_for_irreversiblity,
  const uint32_t old_last_irreversible ) const
{
  return sync_block_writer::find_new_last_irreversible_block( _fork_db, scheduled_witness_objects,
    last_fast_approved_block_by_witness, witnesses_required_for_irreversiblity,
    old_last_irreversible );
}

std::shared_ptr<full_block_type> pruned_block_writer::irreversible_head_block() const
{
  const full_block_object& head_block = get_head_block_data();

  if( head_block.get_num() )
    return head_block.create_full_block();

  return std::shared_ptr<full_block_type>();
}

uint32_t pruned_block_writer::head_block_num( 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  auto head = _fork_db.head();
  return head ? 
    head->get_block_num() : 
    _db.head_block_num();
}

block_id_type pruned_block_writer::head_block_id( 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  auto head = _fork_db.head();
  return head ? 
    head->get_block_id() : 
    _db.head_block_id();
}

std::shared_ptr<full_block_type> pruned_block_writer::read_block_by_num( uint32_t block_num ) const
{
  return retrieve_full_block( block_num );
}

void pruned_block_writer::process_blocks(uint32_t starting_block_number, uint32_t ending_block_number,
  block_processor_t processor, hive::chain::blockchain_worker_thread_pool& thread_pool) const
{
  if( starting_block_number > ending_block_number )
    return;

  uint32_t count = ending_block_number - starting_block_number + 1;
  full_block_vector_t blocks = get_block_range( starting_block_number, count );
  for( const auto& full_block : blocks )
  {
    if( not processor( full_block ) )
      break;
  }
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

bool pruned_block_writer::is_known_irreversible_block( const block_id_type& id ) const
{
  try {
    const auto& idx = _db.get_index< full_block_index, by_hash >();
    auto itr = idx.find( id );
    return itr != idx.end();
  } FC_CAPTURE_AND_RETHROW()
}

bool pruned_block_writer::is_known_block(const block_id_type& id) const
{
  try {
    // Check among reversible blocks in fork db.
    if( _fork_db.fetch_block( id ) )
      return true;

    // Then check among stored full blocks.
    return is_known_irreversible_block( id );
  } FC_CAPTURE_AND_RETHROW()
}

std::deque<block_id_type>::const_iterator pruned_block_writer::find_first_item_not_in_blockchain(
  const std::deque<block_id_type>& item_hashes_received ) const
{
  return fork_db_block_reader::find_first_item_not_in_blockchain( _fork_db, item_hashes_received, 
    [&](const block_id_type& id){ return this->is_known_irreversible_block( id ); } );
}

full_block_vector_t pruned_block_writer::fetch_block_range( 
  const uint32_t starting_block_num, const uint32_t count, 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{ 
  return fork_db_block_reader::fetch_block_range( 
    _fork_db, 
    [&](const uint32_t starting_block_num, const uint32_t count, fc::microseconds wait_for_microseconds)
      ->full_block_vector_t {
        return get_block_range( starting_block_num, count );
    },
    starting_block_num,
    count,
    wait_for_microseconds );
}

const full_block_object& pruned_block_writer::get_head_block_data() const
{
  const auto& idx = _db.get_index< full_block_index, by_num >();
  const full_block_object& head_block_object = *( idx.rbegin() );
  return head_block_object;
}

full_block_vector_t pruned_block_writer::get_block_range( 
  uint32_t starting_block_num, uint32_t count ) const
{ 
  std::vector<const full_block_object*> objects;
  for( ; count > 0; --count, ++starting_block_num )
  {
    const full_block_object* fbo = pruned_block_writer::find_full_block( starting_block_num );
    if( fbo == nullptr )
      break;

    objects.push_back( fbo );
  }

  full_block_vector_t result;
  result.reserve( objects.size() );
  for( const full_block_object* fbo : objects )
  {
    result.push_back( fbo->create_full_block() );
  }  

  return result;
}

block_id_type pruned_block_writer::get_block_id_for_num( uint32_t block_num ) const
{
  const full_block_object* fbo = this->find_full_block( block_num );
  if( fbo != nullptr )
    return fbo->block_id;
  else
    return block_id_type();
}

std::vector<block_id_type> pruned_block_writer::get_blockchain_synopsis( 
  const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point ) const
{
  try
  {
  return fork_db_block_reader::get_blockchain_synopsis( _fork_db, reference_point,
     number_of_blocks_after_reference_point,
    [&](uint32_t block_num)->block_id_type {
      return this->get_block_id_for_num( block_num );
    });
  }
  FC_CAPTURE_AND_RETHROW()
}

std::vector<block_id_type> pruned_block_writer::get_block_ids(
  const std::vector<block_id_type>& blockchain_synopsis, uint32_t& remaining_item_count,
  uint32_t limit) const
{
  return fork_db_block_reader::get_block_ids( _fork_db, blockchain_synopsis,
    remaining_item_count, limit, [&](uint32_t block_num)->block_id_type {
      return this->get_block_id_for_num( block_num );
    });
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

const full_block_object* pruned_block_writer::find_full_block( uint32_t recent_block_num ) const
{
  try
  {
    full_block_object::id_type fbid( recent_block_num % _stored_block_number );
    const full_block_object* fbo = _db.find<full_block_object, by_id>( fbid );
    if( fbo != nullptr )
    {
      if( fbo->byte_size == 0 )
        return nullptr;
        
      uint32_t actual_block_num = block_header::num_from_id( fbo->block_id );
      if( actual_block_num != recent_block_num )
        return nullptr;
    }

    return fbo;
  }
  FC_CAPTURE_AND_RETHROW()
}

std::shared_ptr<full_block_type> pruned_block_writer::retrieve_full_block( uint32_t recent_block_num ) const
{
  try
  {
    const full_block_object* fbo = find_full_block( recent_block_num );
    if( fbo == nullptr )
      return std::shared_ptr<full_block_type>();

    return fbo->create_full_block();
  }
  FC_CAPTURE_AND_RETHROW()
}

} } //hive::chain


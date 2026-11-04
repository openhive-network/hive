#include <hive/chain/single_block_storage.hpp>

#include <hive/chain/database.hpp>

namespace hive { namespace chain {

namespace {
  // While syncing, p2p asks about blocks a peer offered while we applied them from other peers,
  // and about the last block it knows a peer has. Both trail the last irreversible block by at
  // most a few thousand blocks, so that many blocks (1.5 MiB) leave plenty of margin.
  constexpr uint32_t RECENT_BLOCK_IDS_CAPACITY = 0x10000;
}

single_block_storage::single_block_storage( appbase::application& app,
                                            blockchain_worker_thread_pool& thread_pool )
  : _app( app ), _thread_pool( thread_pool ), _recent_block_ids( RECENT_BLOCK_IDS_CAPACITY )
{}

void single_block_storage::open_and_init( const block_log_open_args& bl_open_args, bool read_only,
  bool allow_artifacts_regeneration, database* db )
{
  FC_ASSERT(db);

  _open_args = bl_open_args;
  _db = db;

  // Ids of blocks older than the one we start from are not needed: peers can only offer blocks
  // that follow one we have.
  _recent_block_ids.clear();
  const auto lib = head_block();
  if( lib )
    _recent_block_ids.remember( lib->get_block_id(), lib->get_block_header().timestamp );
}

void single_block_storage::reopen_for_writing()
{
  // Nothing to do here.
}

void single_block_storage::close_storage()
{
  // Nothing to do here.
}

void single_block_storage::append( const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync )
{
  _recent_block_ids.remember( full_block->get_block_id(), full_block->get_block_header().timestamp );
  _db->set_last_irreversible_block_data( full_block );
}

void single_block_storage::flush_head_storage()
{
  // Nothing to do here.
}

void single_block_storage::wipe_storage_files( const fc::path& dir )
{
  // Nothing to do here.
}

std::shared_ptr<full_block_type> single_block_storage::head_block() const
{
  return _db->get_last_irreversible_block_data();
}

uint32_t single_block_storage::head_block_num(
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  const auto lib = head_block();
  return lib ? lib->get_block_num() : 0;
}

block_id_type single_block_storage::head_block_id(
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  const auto lib = head_block();
  return lib ? lib->get_block_id() : block_id_type();
}

std::shared_ptr<full_block_type> single_block_storage::read_block_by_num( uint32_t block_num ) const
{
  const auto lib = head_block();
  return ( lib && lib->get_block_num() == block_num ) ? lib : full_block_ptr_t();
}

void single_block_storage::process_blocks(uint32_t starting_block_number,
  uint32_t ending_block_number, block_processor_t processor,
  hive::chain::blockchain_worker_thread_pool& thread_pool) const
{
  const auto lib = head_block();
  if( lib &&
      lib->get_block_num() >= starting_block_number &&
      lib->get_block_num() <= ending_block_number )
  {
    processor( lib );
  }
}

std::shared_ptr<full_block_type> single_block_storage::fetch_block_by_id(
  const block_id_type& id ) const
{
  try {
    const auto lib = head_block();
    if( lib && lib->get_block_id() == id )
      return lib;

    return full_block_ptr_t();
  } FC_CAPTURE_AND_RETHROW()
}

fc::optional<fc::time_point_sec> single_block_storage::fetch_block_time_by_id(
  const block_id_type& id ) const
{
  const auto lib = head_block();
  if( lib && lib->get_block_id() == id )
    return lib->get_block_header().timestamp;

  const auto recent_block = _recent_block_ids.find( protocol::block_header::num_from_id( id ) );
  if( recent_block && recent_block->id == id )
    return recent_block->timestamp;
  return fc::optional<fc::time_point_sec>();
}

std::shared_ptr<full_block_type> single_block_storage::get_block_by_number( uint32_t block_num,
  fc::microseconds wait_for_microseconds ) const
{
  const auto lib = head_block();
  FC_ASSERT( lib && "get_block_by_number", "Internal error - last irreversible block was NOT set." );
  uint32_t head_block_num = lib->get_block_num();

  if( block_num > head_block_num )
    return full_block_ptr_t();

  // Legacy behavior.
  if( block_num == 0 )
    return full_block_ptr_t();

  FC_ASSERT( block_num == head_block_num,
    "Block ${num} has been pruned (oldest stored block is ${old}). "
    "Consider disabling pruning or increasing block-log-split value (currently 0).",
    ("num", block_num)("old", head_block_num) );

  return lib;
}

std::vector<std::shared_ptr<full_block_type>> single_block_storage::fetch_block_range(
  const uint32_t starting_block_num, const uint32_t count,
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  std::vector<std::shared_ptr<full_block_type>> result;
  const auto lib = head_block();
  if( lib &&
      lib->get_block_num() >= starting_block_num &&
      lib->get_block_num() < starting_block_num + count )
  {
    result.push_back( lib );
  }
  return result;
}

bool single_block_storage::is_known_block(const block_id_type& id) const
{
  const auto lib = head_block();
  if( lib && lib->get_block_id() == id )
    return true;

  const auto recent_block = _recent_block_ids.find( protocol::block_header::num_from_id( id ) );
  return recent_block && recent_block->id == id;
}

std::deque<block_id_type>::const_iterator single_block_storage::find_first_item_not_in_blockchain(
  const std::deque<block_id_type>& item_hashes_received ) const
{
  return std::partition_point(item_hashes_received.begin(), item_hashes_received.end(), [&](const block_id_type& block_id) {
    return is_known_block(block_id);
  });
}

block_id_type single_block_storage::find_block_id_for_num( uint32_t block_num ) const
{
  const auto lib = head_block();
  if( lib && lib->get_block_num() == block_num )
    return lib->get_block_id();

  const auto recent_block = _recent_block_ids.find( block_num );
  if( not recent_block )
    FC_THROW_EXCEPTION( fc::key_not_found_exception,
                        "block number ${block_num} not found", (block_num) );

  return recent_block->id;
}

uint32_t single_block_storage::get_tail_block_num() const
{
  // Only the last irreversible block is kept. Without it no block has been pruned yet.
  const auto lib = head_block();
  return lib ? lib->get_block_num() : 1;
}

std::vector<block_id_type> single_block_storage::get_blockchain_synopsis(
  const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point ) const
{
  const auto lib = head_block();
  FC_ASSERT( lib, "Internal error - last irreversible block was NOT set." );

  uint32_t last_irreversible_block_num = lib->get_block_num();
  uint32_t reference_point_block_num = protocol::block_header::num_from_id(reference_point);
  if (reference_point_block_num < last_irreversible_block_num)
  {
    wlog("Unable to generate a usable synopsis because all irreversible blocks except head one have been pruned.");
    // TODO: get the right type of exception here
    //FC_THROW_EXCEPTION(graphene::net::block_older_than_undo_history, "Peer is on a fork I'm unable to switch to");
    FC_THROW("Unable to switch to a fork due to excessive block pruning.");
  }

  return std::vector<block_id_type>();
}

std::vector<block_id_type> single_block_storage::get_block_ids(
  const std::vector<block_id_type>& blockchain_synopsis, uint32_t& remaining_item_count,
  uint32_t limit) const
{
  remaining_item_count = 0;
  return vector<block_id_type>();
}

} } //hive::chain

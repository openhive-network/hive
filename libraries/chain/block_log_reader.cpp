#include <hive/chain/block_log_reader.hpp>

#include <hive/chain/block_log.hpp>

namespace hive { namespace chain {

block_log_reader::block_log_reader( const block_log& block_log )
  : _block_log( block_log )
{}

void block_log_reader::start_reader( const std::shared_ptr<full_block_type>& head_block )
{
  // nothing to do with block log at this stage.
}

std::shared_ptr<full_block_type> block_log_reader::head_block() const
{
  return _block_log.head();
}

uint32_t block_log_reader::head_block_num( 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  return _block_log.head() ? _block_log.head()->get_block_num() : 0;
}

block_id_type block_log_reader::head_block_id( 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  return _block_log.head() ? _block_log.head()->get_block_id() : block_id_type();
}

std::shared_ptr<full_block_type> block_log_reader::read_block_by_num( uint32_t block_num ) const
{
  return _block_log.read_block_by_num( block_num );
}

void block_log_reader::process_blocks(uint32_t starting_block_number, uint32_t ending_block_number,
  block_processor_t processor) const
{
  _block_log.for_each_block( starting_block_number, ending_block_number, processor, 
                             block_log::for_each_purpose::replay );
}

std::shared_ptr<full_block_type> block_log_reader::fetch_block_by_number( uint32_t block_num,
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{ 
  try {
    return read_block_by_num(block_num);
  } FC_LOG_AND_RETHROW()
}

std::shared_ptr<full_block_type> block_log_reader::fetch_block_by_id( 
  const block_id_type& id ) const
{
  try {
    std::shared_ptr<full_block_type> block = 
      read_block_by_num( protocol::block_header::num_from_id( id ) );
    if( block && block->get_block_id() == id )
      return block;
    return std::shared_ptr<full_block_type>();
  } FC_CAPTURE_AND_RETHROW()
}

bool block_log_reader::is_known_block(const block_id_type& id) const
{
  try {
    auto requested_block_num = protocol::block_header::num_from_id(id);
    auto read_block_id = _block_log.read_block_id_by_num(requested_block_num);

    return read_block_id != block_id_type() && read_block_id == id;
  } FC_CAPTURE_AND_RETHROW()
}

bool block_log_reader::is_known_block_unlocked(const block_id_type& id) const
{ 
  try {
    auto requested_block_num = protocol::block_header::num_from_id(id);
    auto read_block_id = _block_log.read_block_id_by_num(requested_block_num);

    return read_block_id != block_id_type() && read_block_id == id;
  } FC_CAPTURE_AND_RETHROW()
}

std::deque<block_id_type>::const_iterator block_log_reader::find_first_item_not_in_blockchain(
  const std::deque<block_id_type>& item_hashes_received ) const
{
  return std::partition_point(item_hashes_received.begin(), item_hashes_received.end(), [&](const block_id_type& block_id) {
    return is_known_block_unlocked(block_id);
  });
}

block_id_type block_log_reader::find_block_id_for_num( uint32_t block_num )const
{
  block_id_type result;

  try
  {
    if( block_num != 0 )
    {
      result = _block_log.read_block_id_by_num(block_num);
    }
  }
  FC_CAPTURE_AND_RETHROW( (block_num) )

  if( result == block_id_type() )
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "block number not found");

  return result;
}

std::vector<std::shared_ptr<full_block_type>> block_log_reader::fetch_block_range( 
  const uint32_t starting_block_num, const uint32_t count, 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{ 
  try {
    FC_ASSERT(starting_block_num > 0, "Invalid starting block number");
    FC_ASSERT(count > 0, "Why ask for zero blocks?");
    FC_ASSERT(count <= 1000, "You can only ask for 1000 blocks at a time");
    idump((starting_block_num)(count));

    std::vector<std::shared_ptr<full_block_type>> result;
    result = _block_log.read_block_range_by_num(starting_block_num, count);

    idump((result.size()));
    if (!result.empty())
      idump((result.front()->get_block_num())(result.back()->get_block_num()));

    return result;
  } FC_LOG_AND_RETHROW()
}

std::vector<block_id_type> block_log_reader::get_blockchain_synopsis( 
  const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point ) const
{
  //std::vector<block_id_type> synopsis = _fork_db.get_blockchain_synopsis(reference_point, number_of_blocks_after_reference_point, block_number_needed_from_block_log);
  std::vector<block_id_type> synopsis;
  uint32_t last_irreversible_block_num = _block_log.head()->get_block_num();
  uint32_t reference_point_block_num = protocol::block_header::num_from_id(reference_point);
  if (reference_point_block_num < last_irreversible_block_num)
  {
    uint32_t block_number_needed_from_block_log = reference_point_block_num;
    uint32_t reference_point_block_num = protocol::block_header::num_from_id(reference_point);
    auto read_block_id = _block_log.read_block_id_by_num(block_number_needed_from_block_log);

    if (reference_point_block_num == block_number_needed_from_block_log)
    {
      // we're getting this block from the database because it's the reference point,
      // not because it's the last irreversible.
      // We can only do this if the reference point really is in the blockchain
      if (read_block_id == reference_point)
        synopsis.insert(synopsis.begin(), reference_point);
      else
      {
        wlog("Unable to generate a usable synopsis because the peer we're generating it for forked too long ago "
             "(our chains diverge before block #${reference_point_block_num}",
             (reference_point_block_num));
        // TODO: get the right type of exception here
        //FC_THROW_EXCEPTION(graphene::net::block_older_than_undo_history, "Peer is on a fork I'm unable to switch to");
        FC_THROW("Peer is on a fork I'm unable to switch to");
      }
    }
    else
    {
      synopsis.insert(synopsis.begin(), read_block_id);
    }
  }
  return synopsis;
}

std::vector<block_id_type> block_log_reader::get_block_ids(
  const std::vector<block_id_type>& blockchain_synopsis, uint32_t& remaining_item_count,
  uint32_t limit) const
{
  remaining_item_count = 0;
  return vector<block_id_type>();
}

} } //hive::chain


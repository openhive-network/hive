#include <hive/chain/block_log_reader.hpp>

#include <hive/chain/block_log.hpp>

namespace hive { namespace chain {

block_log_reader::block_log_reader( block_log& block_log )
  : _block_log( block_log )
{}

std::shared_ptr<full_block_type> block_log_reader::head() const
{
  return _block_log.head();
}

std::shared_ptr<full_block_type> block_log_reader::read_block_by_num( uint32_t block_num ) const
{
  return _block_log.read_block_by_num( block_num );
}

void block_log_reader::process_blocks(uint32_t starting_block_number, uint32_t ending_block_number,
  block_processor_t processor)
{
  _block_log.for_each_block( starting_block_number, ending_block_number, processor, 
                             block_log::for_each_purpose::replay );
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

} } //hive::chain


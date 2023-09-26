#include <hive/chain/fork_db_block_reader.hpp>

#include <hive/chain/fork_database.hpp>

namespace hive { namespace chain {

fork_db_block_reader::fork_db_block_reader( const fork_database& fork_db, block_log& the_log )
  : block_log_reader( the_log ), _fork_db( fork_db )
{}

bool fork_db_block_reader::is_known_block( const block_id_type& id ) const
{
  try {
    if( _fork_db.fetch_block( id ) )
      return true;

    return block_log_reader::is_known_block( id );
  } FC_CAPTURE_AND_RETHROW()
}

bool fork_db_block_reader::is_known_block_unlocked( const block_id_type& id ) const
{ 
  try {
    if (_fork_db.fetch_block_unlocked(id, true /* only search linked blocks */))
      return true;

    return block_log_reader::is_known_block_unlocked( id );
  } FC_CAPTURE_AND_RETHROW()
}

std::deque<block_id_type>::const_iterator fork_db_block_reader::find_first_item_not_in_blockchain(
  const std::deque<block_id_type>& item_hashes_received ) const
{
  return _fork_db.with_read_lock([&](){
    return std::partition_point(item_hashes_received.begin(), item_hashes_received.end(), [&](const block_id_type& block_id) {
      return is_known_block_unlocked(block_id);
    });
  });
}

block_id_type fork_db_block_reader::find_block_id_for_num( uint32_t block_num )const
{
  block_id_type result;

  try
  {
    if( block_num != 0 )
    {
      // See if fork DB has the item
      shared_ptr<fork_item> fitem = _fork_db.fetch_block_on_main_branch_by_number( block_num );
      if( fitem )
      {
        result = fitem->get_block_id();
      }
      else
      {
        // Next we check if block_log has it. Irreversible blocks are there.
        result = block_log_reader::find_block_id_for_num( block_num );
      }
    }
  }
  FC_CAPTURE_AND_RETHROW( (block_num) )

  if( result == block_id_type() )
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "block number not found");

  return result;
}

} } //hive::chain

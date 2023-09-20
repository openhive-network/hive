#include <hive/chain/sync_block_writer.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/fork_database.hpp>
#include <hive/chain/full_block.hpp>

namespace hive { namespace chain {

sync_block_writer::sync_block_writer( 
  block_log& block_log, fork_database& fork_db )
  : _block_log( block_log ), _fork_db( fork_db )
{}

void sync_block_writer::store_block( uint32_t current_irreversible_block_num,
  uint32_t state_head_block_number )
{
  const auto fork_head = _fork_db.head();
  if (fork_head)
    FC_ASSERT(fork_head->get_block_num() == state_head_block_number,
              "Fork Head Block Number: ${fork_head}, Chain Head Block Number: ${chain_head}",
              ("fork_head", fork_head->get_block_num())("chain_head", state_head_block_number));

  // output to block log based on new last irreverisible block num
  std::shared_ptr<full_block_type> tmp_head = _block_log.head();
  uint32_t blocklog_head_num = tmp_head ? tmp_head->get_block_num() : 0;
  vector<item_ptr> blocks_to_write;

  if( blocklog_head_num < current_irreversible_block_num )
  {
    // Check for all blocks that we want to write out to the block log but don't write any
    // unless we are certain they all exist in the fork db
    while( blocklog_head_num < current_irreversible_block_num )
    {
      item_ptr block_ptr = _fork_db.fetch_block_on_main_branch_by_number( blocklog_head_num + 1 );
      FC_ASSERT( block_ptr, "Current fork in the fork database does not contain the last_irreversible_block" );
      blocks_to_write.push_back( block_ptr );
      blocklog_head_num++;
    }

    for( auto block_itr = blocks_to_write.begin(); block_itr != blocks_to_write.end(); ++block_itr )
      _block_log.append( block_itr->get()->full_block, _is_at_live_sync );

    _block_log.flush();
  }

  // This deletes blocks from the fork db
  _fork_db.set_max_size( state_head_block_number - current_irreversible_block_num + 1 );
}

} } //hive::chain

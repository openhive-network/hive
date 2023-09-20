#include <hive/chain/irreversible_block_writer.hpp>

//#include <hive/chain/block_log.hpp>
//#include <hive/chain/fork_database.hpp>
//#include <hive/chain/full_block.hpp>

namespace hive { namespace chain {

irreversible_block_writer::irreversible_block_writer( 
  block_log& block_log, fork_database& fork_db )
  : _block_log( block_log ), _fork_db( fork_db )
{}

void irreversible_block_writer::store_block( uint32_t current_irreversible_block_num,
  uint32_t state_head_block_number )
{
  /*const auto fork_head = _fork_db.head();
  if (fork_head)
    FC_ASSERT(fork_head->get_block_num() == state_head_block_number,
              "Fork Head Block Number: ${fork_head}, Chain Head Block Number: ${chain_head}",
              ("fork_head", fork_head->get_block_num())("chain_head", state_head_block_number));*/

  // Here was block_log related code, always skipped during replay.

  /*// This deletes blocks from the fork db
  _fork_db.set_max_size( state_head_block_number - current_irreversible_block_num + 1 );*/
}

} } //hive::chain


#include <hive/chain/irreversible_block_writer.hpp>

//#include <hive/chain/block_log.hpp>
//#include <hive/chain/fork_database.hpp>
//#include <hive/chain/full_block.hpp>

namespace hive { namespace chain {

irreversible_block_writer::irreversible_block_writer( 
  block_log& block_log, fork_database& fork_db )
  : _reader( block_log ), _fork_db( fork_db )
{}

block_read_i& irreversible_block_writer::get_block_reader()
{
  return _reader;
}

void irreversible_block_writer::store_block( uint32_t current_irreversible_block_num,
  uint32_t state_head_block_number )
{}

} } //hive::chain


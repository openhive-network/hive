#include <hive/chain/irreversible_block_writer.hpp>

#include <hive/chain/block_log_wrapper.hpp>

namespace hive { namespace chain {

irreversible_block_writer::irreversible_block_writer( const block_log_wrapper& log_wrapper )
  : _block_log_wrapper( log_wrapper )
{}

const block_read_i& irreversible_block_writer::get_block_reader()
{
  return _block_log_wrapper;
}

void irreversible_block_writer::store_block( uint32_t current_irreversible_block_num,
  uint32_t state_head_block_number )
{}

} } //hive::chain


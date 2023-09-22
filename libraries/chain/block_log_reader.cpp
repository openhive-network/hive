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

} } //hive::chain


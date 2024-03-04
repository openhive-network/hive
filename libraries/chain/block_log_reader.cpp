#include <hive/chain/block_log_reader.hpp>

#include <hive/chain/block_log.hpp>

namespace hive { namespace chain {

block_log_reader::block_log_reader( const block_log& block_log )
  : _block_log( block_log )
{}

const block_log& block_log_reader::get_head_block_log() const
{
  return _block_log;
}

const block_log* block_log_reader::get_block_log_corresponding_to( uint32_t block_num ) const
{
  return &_block_log;
}

block_log_reader_common::block_range_t block_log_reader::read_block_range_by_num( uint32_t starting_block_num, uint32_t count ) const
{
  return _block_log.read_block_range_by_num( starting_block_num, count );
}

void block_log_reader::process_blocks(uint32_t starting_block_number, uint32_t ending_block_number,
  block_processor_t processor, hive::chain::blockchain_worker_thread_pool& thread_pool) const
{
  _block_log.for_each_block( starting_block_number, ending_block_number, processor, 
                             block_log::for_each_purpose::replay, thread_pool );
}

} } //hive::chain


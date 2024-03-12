#include <hive/chain/single_file_block_log_writer.hpp>

namespace hive { namespace chain {

single_file_block_log_writer::single_file_block_log_writer( appbase::application& app )
  : _block_log( app )
{}

const block_log& single_file_block_log_writer::get_head_block_log() const
{
  return _block_log;
}

const block_log* single_file_block_log_writer::get_block_log_corresponding_to( uint32_t block_num ) const
{
  return &_block_log;
}

block_log_reader_common::block_range_t single_file_block_log_writer::read_block_range_by_num(
  uint32_t starting_block_num, uint32_t count ) const
{
  return _block_log.read_block_range_by_num( starting_block_num, count );
}

block_log& single_file_block_log_writer::get_head_block_log()
{
  return _block_log;
}

block_log& single_file_block_log_writer::get_log_for_new_block()
{
  return _block_log;
}

void single_file_block_log_writer::open_and_init( const block_log_open_args& bl_open_args,
  blockchain_worker_thread_pool& thread_pool )
{
  _block_log.open_and_init( bl_open_args.data_dir / "block_log",
                            false /*read_only*/,
                            bl_open_args.enable_block_log_compression,
                            bl_open_args.block_log_compression_level,
                            bl_open_args.enable_block_log_auto_fixing,
                            thread_pool );
}

void single_file_block_log_writer::process_blocks(uint32_t starting_block_number,
  uint32_t ending_block_number, block_processor_t processor,
  hive::chain::blockchain_worker_thread_pool& thread_pool) const
{
  _block_log.for_each_block( starting_block_number, ending_block_number, processor, 
                             block_log::for_each_purpose::replay, thread_pool );
}

} } //hive::chain

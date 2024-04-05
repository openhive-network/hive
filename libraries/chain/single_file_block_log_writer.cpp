#include <hive/chain/single_file_block_log_writer.hpp>

namespace hive { namespace chain {

single_file_block_log_writer::single_file_block_log_writer( appbase::application& app,
  blockchain_worker_thread_pool& thread_pool )
  : _thread_pool( thread_pool ), _block_log( app )
{}

const std::shared_ptr<full_block_type> single_file_block_log_writer::get_head_block() const
{
  return _block_log.head();
}

block_id_type single_file_block_log_writer::read_block_id_by_num( uint32_t block_num ) const
{
  return _block_log.read_block_id_by_num( block_num );
}

std::shared_ptr<full_block_type> single_file_block_log_writer::read_block_by_num( uint32_t block_num ) const
{
  return _block_log.read_block_by_num( block_num );
}

block_log_reader_common::block_range_t single_file_block_log_writer::read_block_range_by_num(
  uint32_t starting_block_num, uint32_t count ) const
{
  return _block_log.read_block_range_by_num( starting_block_num, count );
}

void single_file_block_log_writer::open_and_init( const block_log_open_args& bl_open_args )
{
  _block_log.open_and_init( bl_open_args.data_dir / block_log_file_name_info::_legacy_file_name,
                            false /*read_only*/,
                            bl_open_args.enable_block_log_compression,
                            bl_open_args.block_log_compression_level,
                            bl_open_args.enable_block_log_auto_fixing,
                            _thread_pool );
}

void single_file_block_log_writer::open_and_init( const fc::path& path, bool read_only )
{
  _block_log.open( path, _thread_pool, read_only );
}

void single_file_block_log_writer::close_log()
{
  _block_log.close();
}

std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t> single_file_block_log_writer::read_raw_head_block() const
{
  return _block_log.read_raw_head_block();
}

std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> single_file_block_log_writer::read_raw_block_data_by_num(uint32_t block_num) const
{
  return _block_log.read_raw_block_data_by_num( block_num );
}

void single_file_block_log_writer::append( const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync )
{
  _block_log.append( full_block, is_at_live_sync );
}

void single_file_block_log_writer::flush_head_log()
{
  _block_log.flush();
}

uint64_t single_file_block_log_writer::append_raw( uint32_t block_num, const char* raw_block_data,
  size_t raw_block_size, const block_attributes_t& flags, const bool is_at_live_sync )
{
  return _block_log.append_raw( block_num, raw_block_data, raw_block_size, flags, is_at_live_sync );
}

void single_file_block_log_writer::process_blocks(uint32_t starting_block_number,
  uint32_t ending_block_number, block_processor_t processor,
  hive::chain::blockchain_worker_thread_pool& thread_pool) const
{
  _block_log.for_each_block( starting_block_number, ending_block_number, processor, 
                             block_log::for_each_purpose::replay, thread_pool );
}

} } //hive::chain

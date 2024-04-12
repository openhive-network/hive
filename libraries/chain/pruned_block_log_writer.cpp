#include <hive/chain/pruned_block_log_writer.hpp>

namespace hive { namespace chain {

pruned_block_log_writer::pruned_block_log_writer( uint32_t split_file_kept,
  appbase::application& app, blockchain_worker_thread_pool& thread_pool )
  : split_file_block_log_writer( app, thread_pool ), _split_file_kept( split_file_kept )
{
  FC_ASSERT( split_file_kept > 0, "Wrong block log writer." );
}

uint32_t pruned_block_log_writer::validate_tail_part_number( uint32_t tail_part_number, 
  uint32_t head_part_number ) const
{
  FC_ASSERT( head_part_number >= tail_part_number );

  // Require configured number of log file parts, unless we're only starting.
  if( tail_part_number > 1 &&
      head_part_number - tail_part_number < _split_file_kept )
    throw std::runtime_error( 
      "Too few block log part files found (" + std::to_string( head_part_number - tail_part_number ) +
      "), " + std::to_string( _split_file_kept ) + " required." );

  return head_part_number > _split_file_kept ?
          head_part_number - _split_file_kept :
          1;
}

void pruned_block_log_writer::rotate_part_files( uint32_t new_part_number )
{
  FC_ASSERT( new_part_number > 1 ); // Initial part number is 1, new one must be at least 2.

  if( new_part_number -1 > _split_file_kept )
  {
    uint32_t removed_part_number = new_part_number -1 -_split_file_kept; // is > 0
    block_log* removed_log = _logs[ removed_part_number -1 ];
    _logs[ removed_part_number -1 ] = nullptr;
    FC_ASSERT( removed_log != nullptr );
    fc::path log_file = removed_log->get_log_file();
    fc::path artifacts_file = removed_log->get_artifacts_file();
    removed_log->close();
    delete removed_log;
    fc::remove( log_file );
    fc::remove( artifacts_file );
  }
}

std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> pruned_block_log_writer::read_raw_block_data_by_num(uint32_t block_num) const
{
  FC_ASSERT( false, "Not available in pruned block log reader/writer");
}

} } //hive::chain

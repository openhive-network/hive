#include <hive/utilities/split_block_log.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/block_log_wrapper.hpp>

namespace hive { namespace utilities {

using hive::chain::block_log;
using hive::chain::block_log_artifacts;
using hive::chain::block_log_file_name_info;
using hive::chain::block_log_wrapper;
using hive::chain::blockchain_worker_thread_pool;

void split_block_log( fc::path monolith_path, uint32_t head_part_number, size_t part_count,
  appbase::application& app, blockchain_worker_thread_pool& thread_pool,
  const fc::optional<fc::path> splitted_block_log_destination_dir )
{
  std::stringstream request;
  request << "Requested generation of " 
          << ( part_count == 0 ? "all" : std::to_string( part_count ) ) 
          << " part files, up to " 
          << ( head_part_number == 0 ? "head part." : ("part " + std::to_string( head_part_number ) ) );
  ilog( request.str() );

  FC_ASSERT( fc::exists( monolith_path ) && fc::is_regular_file( monolith_path ),
    "${monolith_path} is missing or not a regular file.", (monolith_path) );
  FC_ASSERT( monolith_path.filename().string() == block_log_file_name_info::_legacy_file_name,
    "${monolith_path} is not legacy monolithic block log file.", (monolith_path) );

  ilog( "Opening legacy monolithic block log as source." );
  auto mono_log = block_log_wrapper::create_opened_wrapper( monolith_path, app, thread_pool, 
                                                            true /*read_only*/ );
  uint32_t head_block_num = mono_log->head_block_num();
  idump((head_block_num));

  if( head_part_number == 0 /*determine from source log*/)
  {
    head_part_number = 
      block_log_wrapper::get_part_number_for_block( head_block_num, MAX_FILES_OF_SPLIT_BLOCK_LOG );
    ilog( "head_part_number for block ${head_block_num} is ${head_part_number}", (head_block_num)(head_part_number) );
  }

  ilog( "Actual head_part_number: ${head_part_number}", (head_part_number) );

  uint32_t tail_part_number = 
    ( part_count == 0 /*all*/ || head_part_number <= part_count ) ?
    1 :
    head_part_number - part_count +1;

  ilog( "Actual tail_part_number: ${tail_part_number}", (tail_part_number) );

  fc::path output_path( (splitted_block_log_destination_dir ? *splitted_block_log_destination_dir : monolith_path.parent_path()) );
  fc::directory_iterator it( output_path );
  fc::directory_iterator end_it;
  for( ; it != end_it; it++ )
  {
    uint32_t part_number = block_log_file_name_info::is_part_file( *it );
    FC_ASSERT( part_number < tail_part_number || part_number > head_part_number,
      "Conflicting block log part file ${f} found.", ("f", *it) );
  }

  ilog( "Opening split block log as target." );
  auto split_log = block_log_wrapper::create_limited_wrapper( output_path, app, thread_pool,
                                                              tail_part_number/*start_from_part*/ );

  ilog("Starting splitting");

  uint32_t starting_block_number = block_log_wrapper::get_number_of_first_block_in_part( tail_part_number, MAX_FILES_OF_SPLIT_BLOCK_LOG );
  uint32_t stop_at_block = 
    std::min<uint32_t>( head_block_num, block_log_wrapper::get_number_of_last_block_in_part( head_part_number, MAX_FILES_OF_SPLIT_BLOCK_LOG ) );
  ilog( "Splitting blocks ${starting_block_number} to ${stop_at_block}",
        (starting_block_number)(stop_at_block) );

  uint32_t current_block_number = starting_block_number;

  while( current_block_number <= stop_at_block )
  {
    // read a block
    std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> data_with_artifacts;
    if (current_block_number == head_block_num)
    {
      std::tuple<std::unique_ptr<char[]>, size_t, block_log::block_attributes_t> head_block_data =
        mono_log->read_raw_head_block();
      size_t block_size = std::get<1>(head_block_data);
      block_log_artifacts::artifacts_t block_artifacts(std::get<2>(head_block_data), 0/*dummy*/, 0/*dummy*/);
      data_with_artifacts = 
        std::make_tuple(std::get<0>(std::move(head_block_data)), block_size, std::move(block_artifacts));
    }
    else
    {
      data_with_artifacts = mono_log->read_raw_block_data_by_num(current_block_number);
    }

    // and write it
    size_t raw_block_size                       = std::get<1>(data_with_artifacts);
    const block_log::block_attributes_t& flags  = std::get<2>(data_with_artifacts).attributes;
    split_log->append_raw(current_block_number,
                          std::get<0>(std::move(data_with_artifacts)).get(),
                          raw_block_size,
                          flags,
                          false /*is_at_live_sync*/);

    ++current_block_number;
  }
  
  ilog("Done spliting legacy monolithic block log file.");
  
  // Note that both block logs are closed when their shared_ptr is destructed.
}

} } // hive::utilities

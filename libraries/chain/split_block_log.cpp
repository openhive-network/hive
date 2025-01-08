#include <hive/chain/split_block_log.hpp>
#include <hive/chain/block_log.hpp>
#include <hive/chain/block_log_wrapper.hpp>

namespace hive { namespace chain {

using block_data_t = std::tuple<uint32_t,std::unique_ptr<char[]>,size_t,block_log_artifacts::artifacts_t>;
using block_data_container_t = std::vector<block_data_t>;


void split_block_log( fc::path monolith_path, uint32_t head_block_number, size_t part_count,
  appbase::application& app, blockchain_worker_thread_pool& thread_pool,
  const fc::optional<fc::path> split_block_log_destination_dir )
{
  std::stringstream request;
  request << "Requested generation of " 
          << ( part_count == 0 ? "all" : std::to_string( part_count ) ) 
          << " part files, up to " 
          << ( head_block_number == 0 ? "head block." : ("block " + std::to_string( head_block_number ) ) );
  ilog( request.str() );

  FC_ASSERT( fc::exists( monolith_path ) && fc::is_regular_file( monolith_path ),
    "${monolith_path} is missing or not a regular file.", (monolith_path) );
  FC_ASSERT( monolith_path.filename().string() == block_log_file_name_info::_legacy_file_name,
    "${monolith_path} is not legacy monolithic block log file.", (monolith_path) );

  ilog( "Opening legacy monolithic block log as source." );
  auto mono_log = block_log_wrapper::create_opened_wrapper( monolith_path, app, thread_pool, 
                                                            true /*read_only*/ );
  uint32_t source_head_block_num = mono_log->head_block_num();
  idump((source_head_block_num));

  if( head_block_number == 0 /*determine from source log*/)
  {
    head_block_number = source_head_block_num;
    ilog( "Actual head_block_number is ${head_block_number}", (head_block_number) );
  }

  uint32_t head_part_number = 
      block_log_wrapper::get_part_number_for_block( head_block_number, MAX_FILES_OF_SPLIT_BLOCK_LOG );
  ilog( "Actual head_part_number: ${head_part_number}", (head_part_number) );

  uint32_t tail_part_number = 
    ( part_count == 0 /*all*/ || head_part_number <= part_count ) ?
    1 :
    head_part_number - part_count +1;

  ilog( "Actual tail_part_number: ${tail_part_number}", (tail_part_number) );

  fc::path output_path( (split_block_log_destination_dir ? *split_block_log_destination_dir : monolith_path.parent_path()) );
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
  uint32_t stop_at_block = std::min<uint32_t>( source_head_block_num, head_block_number );
  ilog( "Splitting blocks ${starting_block_number} to ${stop_at_block}",
        (starting_block_number)(stop_at_block) );

  uint32_t batch_first_block_num = starting_block_number;
  uint32_t batch_stop_at_block = 
    stop_at_block == source_head_block_num ? stop_at_block -1 : stop_at_block;
  block_log_artifacts::artifact_container_t plural_of_block_artifacts;
  std::unique_ptr<char[]> the_buffer;
  size_t the_buffer_size = 0;
  while( batch_first_block_num <= batch_stop_at_block )
  {
    uint32_t batch_last_block_num = std::min<uint32_t>( batch_stop_at_block, batch_first_block_num + BLOCKS_IN_BATCH_IO_MODE -1 );
    ilog( "Reading blocks #${batch_first_block_num} to #${batch_last_block_num}",
          (batch_first_block_num)(batch_last_block_num) );
    mono_log->multi_read_raw_block_data( batch_first_block_num, batch_last_block_num,
      plural_of_block_artifacts, the_buffer, the_buffer_size );

    ilog( "Appending a batch of ${count} blocks, starting with block #${batch_first_block_num}",
          ("count", plural_of_block_artifacts.size())(batch_first_block_num) );
    split_log->multi_append_raw( batch_first_block_num, the_buffer, plural_of_block_artifacts );

    batch_first_block_num = batch_last_block_num + 1;
  }

  if( stop_at_block == source_head_block_num )
  {
    ilog("Handling source block log's head block #{source_head_block_num}", (source_head_block_num));
    // read head block data
    std::tuple<std::unique_ptr<char[]>, size_t, block_log::block_attributes_t> head_block_data =
      mono_log->read_raw_head_block();
    size_t block_size = std::get<1>(head_block_data);
    block_log_artifacts::artifacts_t block_artifacts(std::get<2>(head_block_data), 0/*dummy*/, 0/*dummy*/);
    // and write it
    const block_log::block_attributes_t& flags  = block_artifacts.attributes;
    split_log->append_raw(source_head_block_num,
                          std::get<0>(std::move(head_block_data)).get(),
                          block_size,
                          flags,
                          false /*is_at_live_sync*/);
  }

  ilog("Done spliting legacy monolithic block log file.");
  
  // Note that both block logs are closed when their shared_ptr is destructed.
}

} } // hive::utilities

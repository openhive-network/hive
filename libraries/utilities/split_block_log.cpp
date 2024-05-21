#include <hive/utilities/split_block_log.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/block_log_wrapper.hpp>

namespace hive { namespace utilities {

using hive::chain::block_log;
using hive::chain::block_log_artifacts;
using hive::chain::block_log_file_name_info;
using hive::chain::block_log_wrapper;
using hive::chain::blockchain_worker_thread_pool;

void split_block_log( fc::path monolith_path, appbase::application& app,
                      blockchain_worker_thread_pool& thread_pool )
{
  FC_ASSERT( fc::exists( monolith_path ) && fc::is_regular_file( monolith_path ),
    "${monolith_path} is missing or not a regular file.", (monolith_path) );
  FC_ASSERT( monolith_path.filename().string() == block_log_file_name_info::_legacy_file_name,
    "${monolith_path} is not legacy monolithic block log file.", (monolith_path) );

  fc::path output_path( monolith_path.parent_path() );
  fc::directory_iterator it( output_path );
  fc::directory_iterator end_it;
  for( ; it != end_it; it++ )
  {
    FC_ASSERT( block_log_file_name_info::is_part_file( *it ) == 0,
      "Existing block log part file ${f} found.", ("f", *it) );
  }

  ilog( "Opening legacy monolithic block log as source." );
  auto mono_log = block_log_wrapper::create_opened_wrapper( monolith_path, app, thread_pool, 
                                                            true /*read_only*/ );
  ilog( "Opening split block log as target." );
  fc::path first_part_path( output_path / block_log_file_name_info::get_nth_part_file_name( 1 ) );
  auto split_log = block_log_wrapper::create_opened_wrapper( first_part_path, app, thread_pool,
                                                             false /*read_only*/);

  ilog("Starting splitting");

  uint32_t head_block_num = mono_log->head_block_num();
  idump((head_block_num));

  uint32_t starting_block_number = 1;
  uint32_t stop_at_block = head_block_num;
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

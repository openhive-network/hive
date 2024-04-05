#include <hive/chain/block_log_manager.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/single_file_block_log_writer.hpp>
#include <hive/chain/split_file_block_log_writer.hpp>

namespace hive { namespace chain {

namespace detail {
  static std::shared_ptr< block_log_writer_common > create_opened_writer( 
    const fc::path& the_path, bool read_only,
    appbase::application& app, blockchain_worker_thread_pool& thread_pool )
  {
    FC_ASSERT( not fc::exists( the_path ) || fc::is_regular_file( the_path ),
      "Path ${p} does NOT point to regular file.", ("p", the_path) );

    if( the_path.filename().string() == block_log_file_name_info::_legacy_file_name )
    {
      auto writer = std::make_shared< single_file_block_log_writer >( app, thread_pool );
      writer->open_and_init( the_path, read_only );
      return writer;
    }

    if( block_log_file_name_info::is_part_file( the_path ) )
    {
      FC_ASSERT( block_log_file_name_info::get_first_block_num_for_file_name( the_path ) == 1,
                "Expected 1st part file name, not following one (${path})", ("path", the_path) );
      auto writer = std::make_shared< split_file_block_log_writer >( app, thread_pool );
      writer->open_and_init( the_path, read_only );
      return writer;
    }

    FC_THROW_EXCEPTION( fc::parse_error_exception, 
      "Provided block log path ${path} matches neither legacy single file name nor split log part file name pattern.",
      ("path", the_path) );
  }
}

std::shared_ptr< block_log_writer_common > block_log_manager_t::create_writer( int block_log_split,
  appbase::application& app, blockchain_worker_thread_pool& thread_pool )
{
  // Allowed values are -1, 0 and positive.
  if( block_log_split < LEGACY_SINGLE_FILE_BLOCK_LOG )
  {
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Not supported block log split value" );
  }

  switch( block_log_split )
  {
    case LEGACY_SINGLE_FILE_BLOCK_LOG:
      {
      return std::make_shared< single_file_block_log_writer >( app, thread_pool );
      }
      break;
    case MULTIPLE_FILES_FULL_BLOCK_LOG:
      {
      return std::make_shared< split_file_block_log_writer >( app, thread_pool );
      }
      break;
    default: 
      {
        if( block_log_split > MULTIPLE_FILES_FULL_BLOCK_LOG )
          FC_THROW_EXCEPTION( fc::parse_error_exception, "Not implemented block log split value" );
        else
          FC_THROW_EXCEPTION( fc::parse_error_exception, "Not supported block log split value" );
      }
      break;
  }
}

std::shared_ptr< block_log_reader_common > block_log_manager_t::create_opened_reader( 
  const fc::path& input_path, appbase::application& app,
  blockchain_worker_thread_pool& thread_pool, bool recreate_artifacts_if_needed /*= true*/ )
{
  return detail::create_opened_writer( input_path, not recreate_artifacts_if_needed /*read_only*/,
                                       app, thread_pool );
}

std::shared_ptr< block_log_writer_common > block_log_manager_t::create_opened_writer(
  const fc::path& output_path, appbase::application& app,
  blockchain_worker_thread_pool& thread_pool )
{
  return detail::create_opened_writer( output_path, false /*read_only*/, app, thread_pool );
}

} } //hive::chain
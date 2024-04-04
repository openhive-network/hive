#include <hive/chain/block_log_manager.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/single_file_block_log_writer.hpp>
#include <hive/chain/split_file_block_log_writer.hpp>

namespace hive { namespace chain {

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
  const fc::path& input_path,
  appbase::application& app, blockchain_worker_thread_pool& thread_pool )
{
  FC_ASSERT( fc::is_regular_file( input_path ), "Path ${p} does NOT point to regular file.",
    ("p", input_path) );

  if( input_path.filename().string() == block_log_file_name_info::_legacy_file_name )
  {
    auto writer = std::make_shared< single_file_block_log_writer >( app, thread_pool );
    writer->open_and_init( input_path, true /*read_only*/ );
    return writer;
  }

  if( block_log_file_name_info::is_part_file( input_path ) )
  {
    FC_ASSERT( block_log_file_name_info::get_first_block_num_for_file_name( input_path ) == 1,
               "Expected 1st part file name, not following one (${path})", ("path", input_path) );
    auto writer = std::make_shared< split_file_block_log_writer >( app, thread_pool );
    writer->open_and_init( input_path, true /*read_only*/ );
    return writer;
  }

  FC_THROW_EXCEPTION( fc::parse_error_exception, 
    "Provided block log path ${path} matches neither legacy single file name nor split log part file name pattern.",
    ("path", input_path) );
}

} } //hive::chain
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

} } //hive::chain
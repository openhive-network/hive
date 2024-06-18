#include <hive/chain/block_storage_interface.hpp>
             
#include <hive/chain/block_log_wrapper.hpp>
#include <hive/chain/single_block_storage.hpp>

namespace hive { namespace chain {

/*static*/ std::shared_ptr< block_storage_i > block_storage_i::create_storage(
  int block_log_split, appbase::application& app, blockchain_worker_thread_pool& thread_pool )
{
  // Allowed values are -1, 0 and positive.
  if( block_log_split < LEGACY_SINGLE_FILE_BLOCK_LOG ||
      block_log_split > MAX_FILES_OF_SPLIT_BLOCK_LOG )
  {
    FC_THROW_EXCEPTION( fc::parse_error_exception, 
                        "Not supported block log split value ${block_log_split}",
                        (block_log_split) );
  }

  if( block_log_split == 0 )
    return std::make_shared< single_block_storage >( app, thread_pool );
  else
    return std::make_shared< block_log_wrapper >( block_log_split, app, thread_pool );
}

} } //hive::chain

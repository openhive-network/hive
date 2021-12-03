
#include <hive/chain/block_log_file.hpp>

#include <hive/chain/file_operation.hpp>

#include <sys/types.h>
#include <fcntl.h>

namespace hive { namespace chain {

  block_log_file::block_log_file() : storage( storage_description::storage_type::block_log )
  {
  }

  void block_log_file::open( const fc::path& file )
  {
    storage.file = file;

    storage.file_descriptor = ::open(storage.file.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    if( storage.file_descriptor == -1 )
      FC_THROW("Error opening block log file ${filename}: ${error}", ("filename", storage.file)("error", strerror(errno)));
    storage.size = file_operation::get_file_size( storage.file_descriptor );
  }

  void block_log_file::close()
  {
    storage.close();

    head.store( boost::shared_ptr<signed_block>() );
  }

} } // hive::chain

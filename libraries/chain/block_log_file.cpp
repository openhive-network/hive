
#include <hive/chain/block_log_file.hpp>

#include <sys/types.h>
#include <fcntl.h>

namespace hive { namespace chain {

  block_log_file::block_log_file() : storage( storage_description::storage_type::block_log )
  {
  }

  void block_log_file::open( const fc::path& file )
  {
    storage.open( file );
  }

  void block_log_file::close()
  {
    storage.close();

    head.store( boost::shared_ptr<signed_block>() );
  }

} } // hive::chain


#include <hive/chain/storage_description.hpp>

namespace hive { namespace chain {

  storage_description::storage_description( storage_description::storage_type val ): storage( val )
  {
  }

  void storage_description::close()
  {
    if( file_descriptor != -1 )
    {
      ::close(file_descriptor);
      file_descriptor = -1;
    }

    status  = status_type::none;
    size    = 0;
    pos     = 0;
  }

} } // hive::chain

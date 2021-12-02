#pragma once

#include <fc/filesystem.hpp>

namespace hive { namespace chain {

  struct storage_description
  {
    int       file_descriptor = -1;
    fc::path  file;
  };

  struct storage_description_ex : public storage_description
  {
    // only accessed when appending a block, doesn't need locking
    ssize_t block_log_size = 0;
  };

} } // hive::chain

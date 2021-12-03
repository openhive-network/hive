#pragma once

#include <fc/filesystem.hpp>

namespace hive { namespace chain {

  struct storage_description
  {
    enum status_type  : uint32_t { none, reopen, resume };
    enum storage_type : uint32_t { block_log, block_log_idx, hash_idx };

    status_type status = status_type::none;
    const storage_type storage;

    // only accessed when appending a block, doesn't need locking
    ssize_t   size  = 0;
    uint64_t  pos   = 0;

    int       file_descriptor = -1;
    fc::path  file;

    storage_description( storage_type val );

    void close();
  };

} } // hive::chain

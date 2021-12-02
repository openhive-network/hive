#pragma once

#include <hive/protocol/block.hpp>

namespace hive { namespace chain {

  using namespace hive::protocol;

  class file_operation
  {
    public:

      static void write_with_retry(int filedes, const void* buf, size_t nbyte);
      static void pwrite_with_retry(int filedes, const void* buf, size_t nbyte, off_t offset);
      static size_t pread_with_retry(int filedes, void* buf, size_t nbyte, off_t offset);

      static signed_block read_block_from_offset_and_size(int block_log_fd, uint64_t offset, uint64_t size);
  };

} } // hive::chain

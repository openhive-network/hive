#include <hive/chain/file_operation.hpp>

#include <sys/stat.h>

namespace hive { namespace chain {

  void file_operation::write_with_retry(int fd, const void* buf, size_t nbyte)
  {
    for (;;)
    {
      ssize_t bytes_written = write(fd, buf, nbyte); 
      if (bytes_written == -1)
        FC_THROW("Error writing ${nbytes} to file: ${error}", 
                  ("nbyte", nbyte)("error", strerror(errno)));
      if (bytes_written == (ssize_t)nbyte)
        return;
      buf = ((const char*)buf) + bytes_written;
      nbyte -= bytes_written;
    }
  }

  void file_operation::pwrite_with_retry(int fd, const void* buf, size_t nbyte, off_t offset)
  {
    for (;;)
    {
      ssize_t bytes_written = pwrite(fd, buf, nbyte, offset);
      if (bytes_written == -1)
        FC_THROW("Error writing ${nbytes} to file at offset ${offset}: ${error}", 
                  ("nbyte", nbyte)("offset", offset)("error", strerror(errno)));
      if (bytes_written == (ssize_t)nbyte)
        return;
      buf = ((const char*)buf) + bytes_written;
      offset += bytes_written;
      nbyte -= bytes_written;
    }
  }

  size_t file_operation::pread_with_retry(int fd, void* buf, size_t nbyte, off_t offset)
  {
    size_t total_read = 0;
    for (;;)
    {
      ssize_t bytes_read = pread(fd, buf, nbyte, offset);
      if (bytes_read == -1)
        FC_THROW("Error reading ${nbytes} from file at offset ${offset}: ${error}", 
                  ("nbyte", nbyte)("offset", offset)("error", strerror(errno)));

      total_read += bytes_read;

      if (bytes_read == 0 || bytes_read == (ssize_t)nbyte)
        break;

      buf = ((char*)buf) + bytes_read;
      offset += bytes_read;
      nbyte -= bytes_read;
    }

    return total_read;
  }

  signed_block file_operation::read_block_from_offset_and_size(int block_log_fd, uint64_t offset, uint64_t size)
  {
    std::unique_ptr<char[]> serialized_data(new char[size]);
    auto total_read = pread_with_retry(block_log_fd, serialized_data.get(), size, offset);

    FC_ASSERT(total_read == size);

    signed_block block;
    fc::raw::unpack_from_char_array(serialized_data.get(), size, block);
    return block;
  }

  ssize_t file_operation::get_file_size(int fd)
  {
    struct stat file_stats;
    if (fstat(fd, &file_stats) == -1)
      FC_THROW("Error getting size of file: ${error}", ("error", strerror(errno)));
    return file_stats.st_size;
  }

} } // hive::chain

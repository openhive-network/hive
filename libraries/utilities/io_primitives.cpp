#include <hive/utilities/io_primitives.hpp>

#include <fc/exception/exception.hpp>

#include <unistd.h>

namespace hive { namespace utilities {

size_t perform_read(int fd, char* buffer, size_t to_read, size_t offset, const std::string& description)
{
  ssize_t bytes_read = 0;
  size_t total_read = 0;
  while(1)
  {
    bytes_read = pread(fd, buffer + bytes_read, to_read, offset);

    if(bytes_read == -1)
      FC_THROW("Error reading ${nbytes} while performing operation: ${op}: ${error}",
        ("op", description)("nbytes", to_read)("error", strerror(errno)));

    total_read += bytes_read;

    if(bytes_read == 0 || bytes_read == (ssize_t)to_read)
      break;

    to_read -= bytes_read;
    offset += bytes_read;
  }

  return total_read;
}

void perform_write(int fd, const char* buffer, size_t to_write, size_t offset, const std::string& description)
{
  ssize_t total_bytes_written = 0;
  while(1)
  {
    ssize_t bytes_written = pwrite(fd, buffer + total_bytes_written, to_write, offset);

    if(bytes_written == -1)
      FC_THROW("Error writing ${nbytes} while performing operation: ${op}: ${error}",
        ("op", description)("nbytes", to_write)("error", strerror(errno)));

    if(bytes_written == (ssize_t)to_write)
      break;

    to_write -= bytes_written;
    offset += bytes_written;
    total_bytes_written += bytes_written;
  }
}


} } /// namespace hive::utilities
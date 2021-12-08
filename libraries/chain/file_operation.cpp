#include <hive/chain/file_operation.hpp>

#include <sys/stat.h>

#include <fstream>

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

  void file_operation::rewrite(const fc::path& input_file, const fc::path& output_file, uint32_t max_block_num)
  {
    std::ifstream intput_block_stream(input_file.generic_string().c_str(), std::ios::in | std::ios::binary);
    std::ofstream output_block_stream(output_file.generic_string().c_str(), std::ios::out | std::ios::binary | std::ios::app);

    uint64_t pos = 0;
    uint64_t end_pos = 0;

    intput_block_stream.seekg(-sizeof(uint64_t), std::ios::end);
    intput_block_stream.read((char*)&end_pos, sizeof(end_pos));

    intput_block_stream.seekg(pos);

    uint32_t block_num = 0;

    while(pos < end_pos)
    {
      signed_block tmp;
      fc::raw::unpack(intput_block_stream, tmp);
      intput_block_stream.read((char*)&pos, sizeof(pos));

      uint64_t out_pos = output_block_stream.tellp();

      if(out_pos != pos)
        ilog("Block position mismatch");

      auto data = fc::raw::pack_to_vector(tmp);
      output_block_stream.write(data.data(), data.size());
      output_block_stream.write((char*)&out_pos, sizeof(out_pos));

      if(++block_num >= max_block_num)
        break;

      if(block_num % 1000 == 0)
        printf("Rewritten block: %u\r", block_num);
    }
  }

} } // hive::chain

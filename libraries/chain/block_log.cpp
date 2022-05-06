#include <hive/chain/block_log.hpp>
#include <hive/protocol/config.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <fstream>
#include <fc/io/raw.hpp>

#include <appbase/application.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/lock_options.hpp>
#include <boost/scope_exit.hpp>

#include <unistd.h>
#include <boost/smart_ptr/atomic_shared_ptr.hpp>
#include <boost/make_shared.hpp>

#ifdef HAVE_ZSTD
# ifndef ZSTD_STATIC_LINKING_ONLY
#  define ZSTD_STATIC_LINKING_ONLY
# endif
# include <zstd.h>
#endif

#ifdef HAVE_BROTLI
# include <brotli/encode.h>
# include <brotli/decode.h>
#endif

#ifdef HAVE_ZLIB
# include <zlib.h>
#endif

#define MMAP_BLOCK_IO

#ifdef MMAP_BLOCK_IO
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

#define LOG_READ  (std::ios::in | std::ios::binary)
#define LOG_WRITE (std::ios::out | std::ios::binary | std::ios::app)

namespace hive { namespace chain {

  typedef boost::interprocess::scoped_lock< boost::mutex > scoped_lock;

  boost::interprocess::defer_lock_type defer_lock;

  namespace detail {
    class block_log_impl {
      public:
        boost::atomic_shared_ptr<signed_block> head;

        // these don't change after opening, don't need locking
        int block_log_fd;
        int block_index_fd;
        fc::path block_file;
        fc::path index_file;

        static void write_with_retry(int filedes, const void* buf, size_t nbyte);
        static void pwrite_with_retry(int filedes, const void* buf, size_t nbyte, off_t offset);
        static size_t pread_with_retry(int filedes, void* buf, size_t nbyte, off_t offset);

        // only accessed when appending a block, doesn't need locking
        ssize_t block_log_size;

        bool compression_enabled = true;

        signed_block read_block_from_offset_and_size(uint64_t offset, uint64_t size);
        signed_block_header read_block_header_from_offset_and_size(uint64_t offset, uint64_t size);
    };

    void block_log_impl::write_with_retry(int fd, const void* buf, size_t nbyte)
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

    void block_log_impl::pwrite_with_retry(int fd, const void* buf, size_t nbyte, off_t offset)
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

    size_t block_log_impl::pread_with_retry(int fd, void* buf, size_t nbyte, off_t offset)
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

    signed_block block_log_impl::read_block_from_offset_and_size(uint64_t offset, uint64_t size)
    {
      std::unique_ptr<char[]> serialized_data(new char[size]);
      auto total_read = pread_with_retry(block_log_fd, serialized_data.get(), size, offset);

      FC_ASSERT(total_read == size);

      signed_block block;
      fc::raw::unpack_from_char_array(serialized_data.get(), size, block);

      return block;
    }

    signed_block_header block_log_impl::read_block_header_from_offset_and_size(uint64_t offset, uint64_t size)
    {
      std::unique_ptr<char[]> serialized_data(new char[size]);
      auto total_read = pread_with_retry(block_log_fd, serialized_data.get(), size, offset);

      FC_ASSERT(total_read == size);

      signed_block_header block_header;
      fc::raw::unpack_from_char_array(serialized_data.get(), size, block_header);

      return block_header;
    }

  } // end namespace detail

  block_log::block_log() : my( new detail::block_log_impl() )
  {
    my->block_log_fd = -1;
    my->block_index_fd = -1;
  }

  block_log::~block_log()
  {
    if (my->block_log_fd != -1)
      ::close(my->block_log_fd);
    if (my->block_index_fd != -1)
      ::close(my->block_index_fd);
  }

  namespace
  {
    ssize_t get_file_size(int fd)
    {
      struct stat file_stats;
      if (fstat(fd, &file_stats) == -1)
        FC_THROW("Error getting size of file: ${error}", ("error", strerror(errno)));
      return file_stats.st_size;
    }
  }

  void block_log::open( const fc::path& file )
  {
    close();

    my->block_file = file;
    my->index_file = fc::path( file.generic_string() + ".index" );

    my->block_log_fd = ::open(my->block_file.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    if (my->block_log_fd == -1)
      FC_THROW("Error opening block log file ${filename}: ${error}", ("filename", my->block_file)("error", strerror(errno)));
    my->block_index_fd = ::open(my->index_file.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    if (my->block_index_fd == -1)
      FC_THROW("Error opening block index file ${filename}: ${error}", ("filename", my->index_file)("error", strerror(errno)));
    my->block_log_size = get_file_size(my->block_log_fd);
    const ssize_t log_size = my->block_log_size;
    const ssize_t index_size = get_file_size(my->block_index_fd);

    /* On startup of the block log, there are several states the log file and the index file can be
      * in relation to eachother.
      *
      *                          Block Log
      *                     Exists       Is New
      *                 +------------+------------+
      *          Exists |    Check   |   Delete   |
      *   Index         |    Head    |    Index   |
      *    File         +------------+------------+
      *          Is New |   Replay   |     Do     |
      *                 |    Log     |   Nothing  |
      *                 +------------+------------+
      *
      * Checking the heads of the files has several conditions as well.
      *  - If they are the same, do nothing.
      *  - If the index file head is not in the log file, delete the index and replay.
      *  - If the index file head is in the log, but not up to date, replay from index head.
      */
    if( log_size )
    {
      ilog( "Log is nonempty" );
      my->head.exchange(boost::make_shared<signed_block>(read_head()));

      if( index_size )
      {
        ilog( "Index is nonempty" );
        // read the last 8 bytes of the block log to get the offset of the beginning of the head 
        // block
        uint64_t block_pos = 0;
        auto bytes_read = detail::block_log_impl::pread_with_retry(my->block_log_fd, &block_pos, sizeof(block_pos), 
          log_size - sizeof(block_pos));

        FC_ASSERT(bytes_read == sizeof(block_pos));

        // read the last 8 bytes of the block index to get the offset of the beginning of the 
        // head block
        uint64_t index_pos = 0;
        bytes_read = detail::block_log_impl::pread_with_retry(my->block_index_fd, &index_pos, sizeof(index_pos), 
          index_size - sizeof(index_pos));

        FC_ASSERT(bytes_read == sizeof(index_pos));

        if( block_pos < index_pos )
        {
          ilog( "block_pos < index_pos, close and reopen index_stream" );
          ddump((block_pos)(index_pos));
          construct_index();
        }
        else if( block_pos > index_pos )
        {
          ilog( "Index is incomplete" );
          construct_index( true/*resume*/, index_pos );
        }
      }
      else
      {
        ilog( "Index is empty" );
        construct_index();
      }
    }
    else if( index_size )
    {
      ilog( "Index is nonempty, remove and recreate it" );
      if (ftruncate(my->block_index_fd, 0))
        FC_THROW("Error truncating block log: ${error}", ("error", strerror(errno)));
    }
  }

  void block_log::rewrite(const fc::path& input_file, const fc::path& output_file, uint32_t max_block_num)
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

  void block_log::close()
  {
    if (my->block_log_fd != -1) {
      ::close(my->block_log_fd);
      my->block_log_fd = -1;
    }
    if (my->block_index_fd != -1) {
      ::close(my->block_index_fd);
      my->block_log_fd = -1;
    }
    my->head.store(boost::shared_ptr<signed_block>());
  }

  bool block_log::is_open()const
  {
    return my->block_log_fd != -1;
  }

  std::pair<uint64_t, block_log::block_attributes_t> split_block_start_pos_with_flags(uint64_t block_start_pos_with_flags)
  {
    block_log::block_attributes_t attributes;
    attributes.flags = (block_log::block_flags)(block_start_pos_with_flags >> 62);
    if (block_start_pos_with_flags & 0x0100000000000000ull)
      attributes.dictionary_number = (uint8_t)((block_start_pos_with_flags >> 48) & 0xff);
    return std::make_pair(block_start_pos_with_flags & 0x0000ffffffffffffull, attributes);
  }

  uint64_t combine_block_start_pos_with_flags(uint64_t block_start_pos, block_log::block_attributes_t attributes)
  {
    return ((uint64_t)attributes.flags << 62) |
           (attributes.dictionary_number ? 0x0100000000000000ull : 0) |
           ((uint64_t)attributes.dictionary_number.value_or(0) << 48) |
           block_start_pos;
  }

  uint64_t block_log::append_raw(const char* raw_block_data, size_t raw_block_size, block_attributes_t attributes)
  {
    try
    {
      uint64_t block_start_pos = my->block_log_size;
      uint64_t block_start_pos_with_flags = combine_block_start_pos_with_flags(block_start_pos, attributes);

      // what we write to the file is the serialized data, followed by the index of the start of the
      // serialized data.  Append that index so we can do it in a single write.
      size_t block_size_including_start_pos = raw_block_size + sizeof(uint64_t);
      std::unique_ptr<char[]> block_with_start_pos(new char[block_size_including_start_pos]);
      memcpy(block_with_start_pos.get(), raw_block_data, raw_block_size);
      *(uint64_t*)(block_with_start_pos.get() + raw_block_size) = block_start_pos_with_flags;

      detail::block_log_impl::write_with_retry(my->block_log_fd, block_with_start_pos.get(), block_size_including_start_pos);
      my->block_log_size += block_size_including_start_pos;

      // add it to the index
      detail::block_log_impl::write_with_retry(my->block_index_fd, &block_start_pos_with_flags, sizeof(block_start_pos_with_flags));

      return block_start_pos;
    }
    FC_LOG_AND_RETHROW()
  }

  std::tuple<std::unique_ptr<char[]>, size_t, block_log::block_attributes_t> block_log::read_raw_block_data_by_num(uint32_t block_num) const
  {
    uint64_t offsets_with_flags[2] = {0, 0};
    uint64_t offset_in_index = sizeof(uint64_t) * (block_num - 1);
    auto bytes_read = detail::block_log_impl::pread_with_retry(my->block_index_fd, &offsets_with_flags, sizeof(offsets_with_flags),  offset_in_index);
    FC_ASSERT(bytes_read == sizeof(offsets_with_flags));

    uint64_t this_block_start_pos;
    block_attributes_t this_block_attributes;
    std::tie(this_block_start_pos, this_block_attributes) = split_block_start_pos_with_flags(offsets_with_flags[0]);
    uint64_t next_block_start_pos;
    block_attributes_t next_block_attributes;
    std::tie(next_block_start_pos, next_block_attributes) = split_block_start_pos_with_flags(offsets_with_flags[1]);

    uint64_t serialized_data_size = next_block_start_pos - this_block_start_pos - sizeof(uint64_t);

    std::unique_ptr<char[]> serialized_data(new char[serialized_data_size]);
    auto total_read = detail::block_log_impl::pread_with_retry(my->block_log_fd, serialized_data.get(), serialized_data_size, this_block_start_pos);
    FC_ASSERT(total_read == serialized_data_size);

    return std::make_tuple(std::move(serialized_data), serialized_data_size, this_block_attributes);
  }

  // threading guarantees:
  // - this function may only be called by one thread at a time
  // - It is safe to call `append` while any number of other threads 
  //   are reading the block log.
  // There is no real use-case for multiple writers so it's not worth
  // adding a lock to allow it.
  uint64_t block_log::append( const signed_block& b )
  {
    try
    {
      uint64_t block_start_pos;
      std::vector<char> serialized_block = fc::raw::pack_to_vector(b);

      if (my->compression_enabled)
      {
        // here, we'll just use the first available method, assuming brotli > zstd > zlib.
        // in the compress_block_log helper app, we try all three and use the best
        try
        {
          std::tuple<std::unique_ptr<char[]>, size_t> brotli_compressed_block = compress_block_brotli(serialized_block.data(), serialized_block.size());
          block_start_pos = append_raw(std::get<0>(brotli_compressed_block).get(), std::get<1>(brotli_compressed_block), {block_flags::brotli});
        }
        catch (const fc::exception&)
        {
          try
          {
            std::tuple<std::unique_ptr<char[]>, size_t> zstd_compressed_block = compress_block_zstd(serialized_block.data(), serialized_block.size(), b.block_num());
            block_start_pos = append_raw(std::get<0>(zstd_compressed_block).get(), std::get<1>(zstd_compressed_block), {block_flags::zstd});
          }
          catch (const fc::exception&)
          {
            try
            {
              std::tuple<std::unique_ptr<char[]>, size_t> deflate_compressed_block = compress_block_deflate(serialized_block.data(), serialized_block.size());
              block_start_pos = append_raw(std::get<0>(deflate_compressed_block).get(), std::get<1>(deflate_compressed_block), {block_flags::deflate});
            }
            catch (const fc::exception&)
            {
              // all compression methods failed, store the block uncompressed
              block_start_pos = append_raw(serialized_block.data(), serialized_block.size(), {block_flags::uncompressed});
            }
          }
        }
      }
      else // compression not enabled
        block_start_pos = append_raw(serialized_block.data(), serialized_block.size(), {block_flags::uncompressed});

      // update our cached head block
      boost::shared_ptr<signed_block> new_head = boost::make_shared<signed_block>(b);
      my->head.exchange(new_head);

      return block_start_pos;
    }
    FC_LOG_AND_RETHROW()
  }

  // threading guarantees:
  // no restrictions
  void block_log::flush()
  {
    // writes to file descriptors are flushed automatically
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log::decompress_raw_block(const char* raw_block_data, size_t raw_block_size, block_log::block_attributes_t attributes)
  {
    try
    {
      switch (attributes.flags)
      {
      case block_log::block_flags::zstd:
        return block_log::decompress_block_zstd(raw_block_data, raw_block_size, attributes.dictionary_number);
      case block_log::block_flags::brotli:
        return block_log::decompress_block_brotli(raw_block_data, raw_block_size);
      case block_log::block_flags::deflate:
        return block_log::decompress_block_deflate(raw_block_data, raw_block_size);
      case block_log::block_flags::uncompressed:
        {
          std::unique_ptr<char[]> block_data_copy(new char[raw_block_size]);
          memcpy(block_data_copy.get(), raw_block_data, raw_block_size);
          return std::make_tuple(std::move(block_data_copy), raw_block_size);
        }
      default:
        FC_THROW("Unrecognized block_flags in block log");
      }
    }
    catch (const fc::exception& e)
    {
      elog("Fatal error decompressing block from block log: ${e}", (e));
      exit(1);
    }
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log::decompress_raw_block(std::tuple<std::unique_ptr<char[]>, size_t, block_log::block_attributes_t>&& raw_block_data_tuple)
  {
    std::unique_ptr<char[]> raw_block_data = std::get<0>(std::move(raw_block_data_tuple));
    size_t raw_block_size = std::get<1>(raw_block_data_tuple);
    block_log::block_attributes_t attributes = std::get<2>(raw_block_data_tuple);

    // avoid a copy if it's uncompressed
    if (attributes.flags == block_log::block_flags::uncompressed)
      return std::make_tuple(std::move(raw_block_data), raw_block_size);

    return decompress_raw_block(raw_block_data.get(), raw_block_size, attributes);
  }

  optional< signed_block > block_log::read_block_by_num( uint32_t block_num )const
  {
    try
    {
      // first, check if it's the current head block; if so, we can just return it.  If the
      // block number is less than than the current head, it's guaranteed to have been fully
      // written to the log+index
      boost::shared_ptr<signed_block> head_block = my->head.load();
      /// \warning ignore block 0 which is invalid, but old API also returned empty result for it (instead of assert).
      if (block_num == 0 || !head_block || block_num > head_block->block_num())
        return optional<signed_block>();
      if (block_num == head_block->block_num())
        return *head_block;

      // if we're still here, we know that it's in the block log, and the block after it is also
      // in the block log (which means we can determine its size)
      std::tuple<std::unique_ptr<char[]>, size_t> uncompressed_block_data = decompress_raw_block(read_raw_block_data_by_num(block_num));
      signed_block block;
      fc::raw::unpack_from_char_array(std::get<0>(uncompressed_block_data).get(), std::get<1>(uncompressed_block_data), block);
      return block;
    }
    FC_CAPTURE_LOG_AND_RETHROW((block_num))
  }

  optional< signed_block_header > block_log::read_block_header_by_num( uint32_t block_num )const
  {
    try
    {
      // first, check if it's the current head block; if so, we can just return it.  If the
      // block number is less than than the current head, it's guaranteed to have been fully
      // written to the log+index
      boost::shared_ptr<signed_block> head_block = my->head.load();
      /// \warning ignore block 0 which is invalid, but old API also returned empty result for it (instead of assert).
      if (block_num == 0 || !head_block || block_num > head_block->block_num())
        return optional<signed_block>();
      if (block_num == head_block->block_num())
        return *head_block;
      // if we're still here, we know that it's in the block log, and the block after it is also
      // in the block log (which means we can determine its size)

      std::tuple<std::unique_ptr<char[]>, size_t> uncompressed_block_data = decompress_raw_block(read_raw_block_data_by_num(block_num));

      signed_block_header block_header;
      fc::raw::unpack_from_char_array(std::get<0>(uncompressed_block_data).get(), std::get<1>(uncompressed_block_data), block_header);
      return block_header;
    }
    FC_CAPTURE_LOG_AND_RETHROW((block_num))
  }

  vector<signed_block> block_log::read_block_range_by_num( uint32_t first_block_num, uint32_t count )const
  {
    try
    {
      vector<signed_block> result;

      uint32_t last_block_num = first_block_num + count - 1;

      // first, check if the last block we want is the current head block; if so, we can 
      // will use it and then load the previous blocks from the block log
      boost::shared_ptr<signed_block> head_block = my->head.load();
      if (!head_block || first_block_num > head_block->block_num())
        return result; // the caller is asking for blocks after the head block, we don't have them

      // if that head block will be our last block, we want it at the end of our vector,
      // so we'll tack it on at the bottom of this function
      bool last_block_is_head_block = last_block_num == head_block->block_num();
      uint32_t last_block_num_from_disk = last_block_is_head_block ? last_block_num - 1 : last_block_num;

      if (first_block_num <= last_block_num_from_disk)
      {
        // then we need to read blocks from the disk
        uint32_t number_of_blocks_to_read = last_block_num_from_disk - first_block_num + 1;
        uint32_t number_of_offsets_to_read = number_of_blocks_to_read + 1;
        // read all the offsets in one go
        std::unique_ptr<uint64_t[]> offsets_with_flags(new uint64_t[number_of_blocks_to_read + 1]);
        uint64_t offset_of_first_offset = sizeof(uint64_t) * (first_block_num - 1);
        detail::block_log_impl::pread_with_retry(my->block_index_fd, offsets_with_flags.get(), sizeof(uint64_t) * number_of_offsets_to_read,  offset_of_first_offset);

        std::unique_ptr<uint64_t[]> offsets(new uint64_t[number_of_blocks_to_read + 1]);
        std::unique_ptr<block_attributes_t[]> attributes(new block_attributes_t[number_of_blocks_to_read + 1]);
        for (unsigned i = 0; i < number_of_blocks_to_read + 1; ++i)
          std::tie(offsets[i], attributes[i]) = split_block_start_pos_with_flags(offsets_with_flags[i]);

        // then read all the blocks in one go
        uint64_t size_of_all_blocks = offsets[number_of_blocks_to_read] - offsets[0];
        idump((size_of_all_blocks));
        std::unique_ptr<char[]> block_data(new char[size_of_all_blocks]);
        detail::block_log_impl::pread_with_retry(my->block_log_fd, block_data.get(), size_of_all_blocks,  offsets[0]);

        // now deserialize the blocks
        for (uint32_t i = 0; i <= last_block_num_from_disk - first_block_num; ++i)
        {
          uint64_t offset_in_memory = offsets[i] - offsets[0];
          uint64_t size = offsets[i + 1] - offsets[i] - sizeof(uint64_t);

          std::tuple<std::unique_ptr<char[]>, size_t> decompressed_raw_block = decompress_raw_block(block_data.get() + offset_in_memory, size, attributes[i]);

          signed_block block;
          fc::raw::unpack_from_char_array(std::get<0>(decompressed_raw_block).get(), std::get<1>(decompressed_raw_block), block);
          result.push_back(std::move(block));
        }
      }

      if (last_block_is_head_block)
        result.push_back(*head_block);
      return result;
    }
    FC_CAPTURE_LOG_AND_RETHROW((first_block_num)(count))
  }

  // not thread safe, but it's only called when opening the block log, we can assume we're the only thread accessing it
  signed_block block_log::read_head()const
  {
    try
    {
      ssize_t block_log_size = get_file_size(my->block_log_fd);

      // read the last int64 of the block log into `head_block_offset`, 
      // that's the index of the start of the head block
      FC_ASSERT(block_log_size >= (ssize_t)sizeof(uint64_t));
      uint64_t head_block_offset_with_flags;
      detail::block_log_impl::pread_with_retry(my->block_log_fd, &head_block_offset_with_flags, sizeof(head_block_offset_with_flags), 
                                               block_log_size - sizeof(head_block_offset_with_flags));
      uint64_t head_block_offset;

      block_attributes_t attributes;
      std::tie(head_block_offset, attributes) = split_block_start_pos_with_flags(head_block_offset_with_flags);
      size_t raw_data_size = block_log_size - head_block_offset - sizeof(head_block_offset);

      std::unique_ptr<char[]> raw_data(new char[raw_data_size]);
      auto total_read = detail::block_log_impl::pread_with_retry(my->block_log_fd, raw_data.get(), raw_data_size, head_block_offset);
      FC_ASSERT(total_read == raw_data_size);

      std::tuple<std::unique_ptr<char[]>, size_t> uncompressed_block_data = decompress_raw_block(std::make_tuple(std::move(raw_data), raw_data_size, attributes));

      signed_block block;
      fc::raw::unpack_from_char_array(std::get<0>(uncompressed_block_data).get(), std::get<1>(uncompressed_block_data), block);
      return block;
    }
    FC_LOG_AND_RETHROW()
  }

  const boost::shared_ptr<signed_block> block_log::head()const
  {
    return my->head.load();
  }

  void block_log::construct_index( bool resume /* = false */, uint64_t index_pos /* = 0 */)
  {
    try
    {
      // the caller has already loaded the head block
      boost::shared_ptr<signed_block> head_block = my->head.load();
      FC_ASSERT(head_block);
      const uint32_t block_num = head_block->block_num();
      idump((block_num));

#define USE_BACKWARDS_INDEX
#ifdef USE_BACKWARDS_INDEX
      // Note: the old implementation recreated the block index by reading the log
      // forwards, starting at the start of the block log and deserializing each block
      // and then writing the new file position to the index.
      // The new implementation recreates the index by reading the block log backwards,
      // extracting only the offsets from the block log.  This should be much more efficient 
      // when regenerating the whole index, but it lacks the ability to repair the end
      // of the index.  
      // It would probably be worthwhile to use the old method if the number of
      // entries to repair is small compared to the total size of the block log
#ifdef MMAP_BLOCK_IO      
      ilog( "Reconstructing Block Log Index using memory-mapped IO...resume=${resume},index_pos=${index_pos}",("resume",resume)("index_pos",index_pos) );
#else
      ilog( "Reconstructing Block Log Index...resume=${resume},index_pos=${index_pos}",("resume",resume)("index_pos",index_pos) );
#endif
      //close old index file if open, we'll reopen after we replace it
      ::close(my->block_index_fd);

      //create and size the new temporary index file (block_log.index.new)
      fc::path new_index_file(my->index_file.generic_string() + ".new");
      const size_t block_index_size = block_num * sizeof(uint64_t);
      int new_index_fd = ::open(new_index_file.generic_string().c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
      if (new_index_fd == -1)
        FC_THROW("Error opening temporary new index file ${filename}: ${error}", ("filename", new_index_file.generic_string())("error", strerror(errno)));
      if (ftruncate(new_index_fd, block_index_size) == -1)
        FC_THROW("Error resizing rebuilt block index file: ${error}", ("error", strerror(errno)));

#ifdef MMAP_BLOCK_IO
      //memory map for block log
      char* block_log_ptr = (char*)mmap(0, my->block_log_size, PROT_READ, MAP_SHARED, my->block_log_fd, 0);
      if (block_log_ptr == (char*)-1)
        FC_THROW("Failed to mmap block log file: ${error}",("error",strerror(errno)));
      if (madvise(block_log_ptr, my->block_log_size, MADV_WILLNEED) == -1)
        wlog("madvise failed: ${error}",("error",strerror(errno)));

      //memory map for index file
      char* block_index_ptr = (char*)mmap(0, block_index_size, PROT_WRITE, MAP_SHARED, new_index_fd, 0);
      if (block_index_ptr == (char*)-1)
        FC_THROW("Failed to mmap block log index: ${error}",("error",strerror(errno)));
#endif
      // now walk backwards through the block log reading the starting positions of the blocks
      // and writing them to the index
      uint32_t block_index = block_num;
      uint64_t block_log_offset_of_block_pos = my->block_log_size - sizeof(uint64_t);
      while (!appbase::app().is_interrupt_request() && block_index)
      {
        // read the file offset of the start of the block from the block log
        uint64_t block_pos;
        uint64_t higher_block_pos;
        higher_block_pos = block_log_offset_of_block_pos;
#ifdef MMAP_BLOCK_IO
        //read next block pos offset from the block log
        memcpy(&block_pos, block_log_ptr + block_log_offset_of_block_pos, sizeof(block_pos));
        // write it to the right location in the new index file
        memcpy(block_index_ptr + sizeof(block_pos) * (block_index - 1), &block_pos, sizeof(block_pos));
#else
        //read next block pos offset from the block log
        detail::block_log_impl::pread_with_retry(my->block_log_fd, &block_pos, sizeof(block_pos), block_log_offset_of_block_pos);
        // write it to the right location in the new index file
        detail::block_log_impl::pwrite_with_retry(new_index_fd, &block_pos, sizeof(block_pos), sizeof(block_pos) * (block_index - 1));
#endif
        if (higher_block_pos <= block_pos) //this is a sanity check on index values stored in the block log
          FC_THROW("bad block index at block ${block_index} because ${higher_block_pos} <= ${block_pos}",
                   ("block_index",block_index)("higher_block_pos",higher_block_pos)("block_pos",block_pos));

        --block_index;
        block_log_offset_of_block_pos = block_pos - sizeof(uint64_t);
      } //while writing block index
      if (appbase::app().is_interrupt_request())
      {
        ilog("Creating Block Log Index was interrupted on user request and can't be resumed. Last applied: (block number: ${n})", ("n", block_num));
        return;
      }

#ifdef MMAP_BLOCK_IO
      if (munmap(block_log_ptr, my->block_log_size) == -1)
        elog("error unmapping block_log: ${error}",("error",strerror(errno)));
      if (munmap(block_index_ptr, block_index_size) == -1)
        elog("error unmapping block_index: ${error}",("error",strerror(errno)));
#endif  
      ::close(new_index_fd);
      fc::remove_all(my->index_file);
      fc::rename(new_index_file, my->index_file);
#else //NOT USE_BACKWARDS_INDEX
      ilog( "Reconstructing Block Log Index in forward direction (old slower way, but allows for interruption)..." );
      // note: this does not handle compressed blocks
      std::fstream block_stream;
      std::fstream index_stream;

      if( !resume )
        fc::remove_all( my->index_file );

      block_stream.open( my->block_file.generic_string().c_str(), LOG_READ );
      index_stream.open( my->index_file.generic_string().c_str(), LOG_WRITE );

      uint64_t pos = resume ? index_pos : 0;
      uint64_t end_pos;

      block_stream.seekg( -sizeof( uint64_t), std::ios::end );      
      block_stream.read( (char*)&end_pos, sizeof( end_pos ) );      
      signed_block tmp;

      block_stream.seekg( pos );
      if( resume )
      {
        index_stream.seekg( 0, std::ios::end );

        fc::raw::unpack( block_stream, tmp );
        block_stream.read( (char*)&pos, sizeof( pos ) );

        ilog("Resuming Block Log Index. Last applied: ( block number: ${n} )( trx: ${trx} )( bytes position: ${pos} )",
                                                            ( "n", tmp.block_num() )( "trx", tmp.id() )( "pos", pos ) );
      }

      while( !appbase::app().is_interrupt_request() && pos < end_pos )
      {
        fc::raw::unpack( block_stream, tmp );
        block_stream.read( (char*)&pos, sizeof( pos ) );
        index_stream.write( (char*)&pos, sizeof( pos ) );
      }

      if( appbase::app().is_interrupt_request() )
        ilog("Creating Block Log Index is interrupted on user request. Last applied: ( block number: ${n} )( trx: ${trx} )( bytes position: ${pos} )",
                                                            ( "n", tmp.block_num() )( "trx", tmp.id() )( "pos", pos ) );

      /// Flush and reopen to be sure that given index file has been saved.
      /// Otherwise just executed replay, next stopped by ctrl+C can again corrupt this file. 
      index_stream.close();
      ::close(my->block_index_fd);
#endif //NOT USE_BACKWARD_INDEX

      ilog("opening new block index");
      my->block_index_fd = ::open(my->index_file.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
      if (my->block_index_fd == -1)
        FC_THROW("Error opening block index file ${filename}: ${error}", ("filename", my->index_file)("error", strerror(errno)));
      //report size of new index file and verify it is the right size for the blocks in block log
      struct stat block_index_stat;
      if (fstat(my->block_index_fd, &block_index_stat) == -1)
        elog("error: could not get size of block log index");
      idump((block_index_stat.st_size));
      FC_ASSERT(block_index_stat.st_size/sizeof(uint64_t) == block_num);
    }
    FC_LOG_AND_RETHROW()
  }

  void block_log::set_compression(bool enabled)
  {
    my->compression_enabled = enabled;
  }

  std::tuple<std::unique_ptr<char[]>, size_t> compress_block_zstd_helper(const char* uncompressed_block_data, 
                                                                         size_t uncompressed_block_size,
                                                                         fc::optional<uint8_t> dictionary_number,
                                                                         ZSTD_CCtx* compression_context,
                                                                         fc::optional<int> compression_level)
  {
#ifdef HAVE_ZSTD
    if (dictionary_number)
    {
      ZSTD_CDict* compression_dictionary = get_zstd_compression_dictionary(*dictionary_number, compression_level.value_or(ZSTD_CLEVEL_DEFAULT));
      size_t ref_cdict_result = ZSTD_CCtx_refCDict(compression_context, compression_dictionary);
      if (ZSTD_isError(ref_cdict_result))
        FC_THROW("Error loading compression dictionary into context");
    }
    else
    {
      ZSTD_CCtx_setParameter(compression_context, ZSTD_c_compressionLevel, compression_level.value_or(ZSTD_CLEVEL_DEFAULT));
    }

    // prevent zstd from adding a 4-byte magic number at the beginning of each block, it serves no purpose here other than wasting 4 bytes
    ZSTD_CCtx_setParameter(compression_context, ZSTD_c_format, ZSTD_f_zstd1_magicless);
    // prevent zstd from adding the decompressed size at the beginning of each block.  For most blocks (those >= 256 bytes),
    // this will save a byte in the encoded output.  For smaller blocks, it does no harm.
    ZSTD_CCtx_setParameter(compression_context, ZSTD_c_contentSizeFlag, 0);
    // prevent zstd from encoding the number of the dictionary used for encoding the block.  We store this in the position/flags, so 
    // we can save another byte here
    ZSTD_CCtx_setParameter(compression_context, ZSTD_c_dictIDFlag, 0);

    // tell zstd not to add a 4-byte checksum at the end of the output.  This is currently the default, so should have no effect
    ZSTD_CCtx_setParameter(compression_context, ZSTD_c_checksumFlag, 0);

    size_t zstd_max_size = ZSTD_compressBound(uncompressed_block_size);
    std::unique_ptr<char[]> zstd_compressed_data(new char[zstd_max_size]);

#if 1
    size_t zstd_compressed_size = ZSTD_compress2(compression_context, 
                                                 zstd_compressed_data.get(), zstd_max_size,
                                                 uncompressed_block_data, uncompressed_block_size);
#else
    size_t zstd_compressed_size = ZSTD_compress_usingCDict(compression_context,
                                                           zstd_compressed_data.get(), zstd_max_size,
                                                           uncompressed_block_data, uncompressed_block_size,
                                                           get_zstd_compression_dictionary(*dictionary_number, compression_level.value_or(ZSTD_CLEVEL_DEFAULT)));
#endif

    if (ZSTD_isError(zstd_compressed_size))
      FC_THROW("Error compressing block with zstd");


    return std::make_tuple(std::move(zstd_compressed_data), zstd_compressed_size);
#endif
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log::compress_block_zstd(const char* uncompressed_block_data, 
                                                                                          size_t uncompressed_block_size,
                                                                                          fc::optional<uint8_t> dictionary_number,
                                                                                          fc::optional<int> compression_level,
                                                                                          fc::optional<ZSTD_CCtx*> compression_context_for_reuse)
  {
#ifdef HAVE_ZSTD
    if (compression_context_for_reuse)
    {
      ZSTD_CCtx_reset(*compression_context_for_reuse, ZSTD_reset_session_and_parameters);
      return compress_block_zstd_helper(uncompressed_block_data, 
                                        uncompressed_block_size,
                                        dictionary_number,
                                        *compression_context_for_reuse,
                                        compression_level);
    }

    ZSTD_CCtx* compression_context = ZSTD_createCCtx();
    BOOST_SCOPE_EXIT(&compression_context) {
      ZSTD_freeCCtx(compression_context);
    } BOOST_SCOPE_EXIT_END

    return compress_block_zstd_helper(uncompressed_block_data, 
                                      uncompressed_block_size,
                                      dictionary_number,
                                      compression_context,
                                      compression_level);
#else
    FC_THROW("hived was not configured with zstd support");
#endif
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log::compress_block_brotli(const char* uncompressed_block_data, 
                                                                                            size_t uncompressed_block_size,
                                                                                            fc::optional<int> compression_quality)
  {
#ifdef HAVE_BROTLI
    size_t brotli_compressed_size = BrotliEncoderMaxCompressedSize(uncompressed_block_size);
    std::unique_ptr<char[]> brotli_compressed_data(new char[brotli_compressed_size]);
    if (!BrotliEncoderCompress(compression_quality.value_or(BROTLI_DEFAULT_QUALITY), BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE,
                               uncompressed_block_size, (const uint8_t*)uncompressed_block_data, 
                               &brotli_compressed_size, (uint8_t*)brotli_compressed_data.get()))
      FC_THROW("Error compressing block with brotli");
    return std::make_tuple(std::move(brotli_compressed_data), brotli_compressed_size);
#else
    FC_THROW("hived was not configured with brotli support");
#endif
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log::compress_block_deflate(const char* uncompressed_block_data, 
                                                                                             size_t uncompressed_block_size,
                                                                                             fc::optional<int> compression_level)
  {
#ifdef HAVE_ZLIB
    z_stream deflate_stream;
    memset(&deflate_stream, 0, sizeof(deflate_stream));
    if (deflateInit(&deflate_stream, compression_level.value_or(Z_DEFAULT_COMPRESSION)) == Z_OK)
    {
      size_t zlib_max_size = deflateBound(&deflate_stream, uncompressed_block_size);
      std::unique_ptr<char[]> zlib_compressed_data(new char[zlib_max_size]);
      deflate_stream.total_in = deflate_stream.avail_in = uncompressed_block_size;
      deflate_stream.avail_out = zlib_max_size;
      deflate_stream.next_in = (Bytef*)uncompressed_block_data;
      deflate_stream.next_out = (Bytef*)zlib_compressed_data.get();
      if (deflate(&deflate_stream, Z_FINISH) == Z_STREAM_END)
      {
        size_t zlib_compressed_size = deflate_stream.total_out;
        deflateEnd(&deflate_stream);
        return std::make_tuple(std::move(zlib_compressed_data), zlib_compressed_size);
      }
      deflateEnd(&deflate_stream);
    }
    FC_THROW("Error compressing block with zlib (deflate)");
#else
    FC_THROW("hived was not configured with zlib support");
#endif
  }

  std::tuple<std::unique_ptr<char[]>, size_t> decompress_block_zstd_helper(const char* compressed_block_data,
                                                                           size_t compressed_block_size,
                                                                           fc::optional<uint8_t> dictionary_number,
                                                                           ZSTD_DCtx* decompression_context)
  {
    if (dictionary_number)
    {
      ZSTD_DDict* decompression_dictionary = get_zstd_decompression_dictionary(*dictionary_number);
      size_t ref_ddict_result = ZSTD_DCtx_refDDict(decompression_context, decompression_dictionary);
      if (ZSTD_isError(ref_ddict_result))
        FC_THROW("Error loading decompression dictionary into context");
    }

    // tell zstd not to expect the first four bytes to be a magic number
    ZSTD_DCtx_setParameter(decompression_context, ZSTD_d_format, ZSTD_f_zstd1_magicless);

    std::unique_ptr<char[]> uncompressed_block_data(new char[HIVE_MAX_BLOCK_SIZE]);
    size_t uncompressed_block_size = ZSTD_decompressDCtx(decompression_context,
                                                         uncompressed_block_data.get(), HIVE_MAX_BLOCK_SIZE,
                                                         compressed_block_data, compressed_block_size);
    if (ZSTD_isError(uncompressed_block_size))
      FC_THROW("Error decompressing block with zstd");
    return std::make_tuple(std::move(uncompressed_block_data), uncompressed_block_size);
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log::decompress_block_zstd(const char* compressed_block_data,
                                                                                            size_t compressed_block_size,
                                                                                            fc::optional<uint8_t> dictionary_number,
                                                                                            fc::optional<ZSTD_DCtx*> decompression_context_for_reuse)
  {
#ifdef HAVE_ZSTD
    if (decompression_context_for_reuse)
    {
      ZSTD_DCtx_reset(*decompression_context_for_reuse, ZSTD_reset_session_and_parameters);
      return decompress_block_zstd_helper(compressed_block_data, 
                                          compressed_block_size,
                                          dictionary_number,
                                          *decompression_context_for_reuse);
    }

    ZSTD_DCtx* decompression_context = ZSTD_createDCtx();
    BOOST_SCOPE_EXIT(&decompression_context) {
      ZSTD_freeDCtx(decompression_context);
    } BOOST_SCOPE_EXIT_END

    return decompress_block_zstd_helper(compressed_block_data, 
                                        compressed_block_size,
                                        dictionary_number,
                                        decompression_context);

#else
    FC_THROW("hived was not configured with zstd support");
#endif
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log::decompress_block_brotli(const char* compressed_block_data, 
                                                                                              size_t compressed_block_size)
  {
#ifdef HAVE_BROTLI
    std::unique_ptr<char[]> uncompressed_block_data(new char[HIVE_MAX_BLOCK_SIZE]);
    size_t uncompressed_block_size = HIVE_MAX_BLOCK_SIZE;
    if (BrotliDecoderDecompress(compressed_block_size, (const uint8_t*)compressed_block_data,
                                &uncompressed_block_size, (uint8_t*)uncompressed_block_data.get()) != BROTLI_DECODER_RESULT_SUCCESS)
      FC_THROW("Error decompressing block with brotli");
    return std::make_tuple(std::move(uncompressed_block_data), uncompressed_block_size);
#else
    FC_THROW("hived was not configured with brotli support");
#endif
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log::decompress_block_deflate(const char* compressed_block_data, 
                                                                                               size_t compressed_block_size)
  {
#ifdef HAVE_ZLIB
    z_stream inflate_stream;
    memset(&inflate_stream, 0, sizeof(inflate_stream));
    if (inflateInit(&inflate_stream) == Z_OK)
    {
      std::unique_ptr<char[]> uncompressed_block_data(new char[HIVE_MAX_BLOCK_SIZE]);
      inflate_stream.next_in = (Bytef*)compressed_block_data;
      inflate_stream.avail_in = compressed_block_size;
      inflate_stream.next_out = (Bytef*)uncompressed_block_data.get(); 
      inflate_stream.avail_out = HIVE_MAX_BLOCK_SIZE;
      if (inflate(&inflate_stream, Z_FINISH) == Z_STREAM_END)
      {
        size_t uncompressed_block_size = inflate_stream.total_out;
        inflateEnd(&inflate_stream);
        return std::make_tuple(std::move(uncompressed_block_data), uncompressed_block_size);
      }
    }
    FC_THROW("Error decompressing block with zlib (deflate)");
#else
    FC_THROW("hived was not configured with zlib support");
#endif
  }

} } // hive::chain

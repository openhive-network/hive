#include <hive/chain/block_log.hpp>
#include <hive/protocol/config.hpp>

#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/detail/block_attributes.hpp>

#include <fstream>
#include <fc/io/raw.hpp>

#include <appbase/application.hpp>

#include <hive/utilities/io_primitives.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/lock_options.hpp>
#include <boost/scope_exit.hpp>

#include <unistd.h>

#ifndef ZSTD_STATIC_LINKING_ONLY
# define ZSTD_STATIC_LINKING_ONLY
#endif
#include <zstd.h>

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
        std::shared_ptr<full_block_type> head;

        // these don't change after opening, don't need locking
        int block_log_fd;
        fc::path block_file;
        block_log_artifacts::block_log_artifacts_ptr_t _artifacts;

        static void write_with_retry(int filedes, const void* buf, size_t nbyte);
        static void pwrite_with_retry(int filedes, const void* buf, size_t nbyte, off_t offset);
        static size_t pread_with_retry(int filedes, void* buf, size_t nbyte, off_t offset);

        // only accessed when appending a block, doesn't need locking
        ssize_t block_log_size;

        bool compression_enabled = true;

        // during testing (around block 63M) we found level 15 to be a good balance between ratio 
        // and compression/decompression times of ~3.5ms & 65μs, so we're making level 15 the default, and the 
        // dictionaries are optimized for level 15
        int zstd_level = 15; 

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
                   ("nbytes", nbyte)("error", strerror(errno)));
        if (bytes_written == (ssize_t)nbyte)
          return;
        buf = ((const char*)buf) + bytes_written;
        nbyte -= bytes_written;
      }
    }

    void block_log_impl::pwrite_with_retry(int fd, const void* buf, size_t nbyte, off_t offset)
    {
      hive::utilities::perform_write(fd, reinterpret_cast<const char*>(buf), nbyte, offset, "saving block log data");
    }

    size_t block_log_impl::pread_with_retry(int fd, void* buf, size_t nbyte, off_t offset)
    {
      return hive::utilities::perform_read(fd, reinterpret_cast<char*>(buf), nbyte, offset, "loading block log data");
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
  }

  block_log::~block_log()
  {
    if (my->block_log_fd != -1)
      ::close(my->block_log_fd);
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

  void block_log::open(const fc::path& file, bool read_only /* = false */, bool auto_open_artifacts /*= true*/ )
  {
      close();

      my->block_file = file;

      int flags = O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC;
      if (read_only)
        flags = O_RDONLY | O_CLOEXEC;
      ilog("Opening blocklog ${blocklog_filename}",("blocklog_filename",my->block_file.generic_string().c_str()));
      my->block_log_fd = ::open(my->block_file.generic_string().c_str(), flags, 0644);
      if (my->block_log_fd == -1)
        FC_THROW("Error opening block log file ${filename}: ${error}", ("filename", my->block_file)("error", strerror(errno)));
      my->block_log_size = get_file_size(my->block_log_fd);
      const ssize_t block_log_size = my->block_log_size;

      uint32_t head_block_num = 0;

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
      if (block_log_size)
      {
        idump((block_log_size));
        std::atomic_store(&my->head, read_head());
        head_block_num = std::atomic_load(&my->head)->get_block_num();
      }

      if (auto_open_artifacts)
        my->_artifacts = block_log_artifacts::open(file, read_only, *this, head_block_num);
  }

  void block_log::close()
  {
    my->_artifacts.reset(); /// Destruction also performs file close.

    if (my->block_log_fd != -1) {
      ::close(my->block_log_fd);
      my->block_log_fd = -1;
    }
    std::atomic_store(&my->head, std::shared_ptr<full_block_type>());
  }

  bool block_log::is_open()const
  {
    return my->block_log_fd != -1;
  }
  
  uint64_t block_log::append_raw(uint32_t block_num, const char* raw_block_data, size_t raw_block_size, const block_attributes_t& attributes)
  {
    std::unique_ptr<char[]> compressed_block_data(new char[raw_block_size]);
    memcpy(compressed_block_data.get(), raw_block_data, raw_block_size);

    std::shared_ptr<full_block_type> full_block;
    if (attributes.flags == block_flags::uncompressed)
      full_block = full_block_type::create_from_uncompressed_block_data(std::move(compressed_block_data), raw_block_size);
    else
      full_block = full_block_type::create_from_compressed_block_data(std::move(compressed_block_data), raw_block_size, attributes);

    return append_raw(block_num, raw_block_data, raw_block_size, attributes, full_block->get_block_id());
  }

  uint64_t block_log::append_raw(uint32_t block_num, const char* raw_block_data, size_t raw_block_size, const block_attributes_t& attributes, const block_id_type& block_id)
  {
    uint64_t block_start_pos = my->block_log_size;
    uint64_t block_start_pos_with_flags = detail::combine_block_start_pos_with_flags(block_start_pos, attributes);

    try
    {
      // what we write to the file is the serialized data, followed by the index of the start of the
      // serialized data.  Append that index so we can do it in a single write.
      size_t block_size_including_start_pos = raw_block_size + sizeof(uint64_t);
      std::unique_ptr<char[]> block_with_start_pos(new char[block_size_including_start_pos]);
      memcpy(block_with_start_pos.get(), raw_block_data, raw_block_size);
      *(uint64_t*)(block_with_start_pos.get() + raw_block_size) = block_start_pos_with_flags;

      detail::block_log_impl::write_with_retry(my->block_log_fd, block_with_start_pos.get(), block_size_including_start_pos);
      my->block_log_size += block_size_including_start_pos;

      my->_artifacts->store_block_artifacts(block_num, block_start_pos, attributes, block_id);

      return block_start_pos;
    }
    FC_CAPTURE_LOG_AND_RETHROW((block_num)(block_start_pos)(block_start_pos_with_flags)(attributes))
  }

  std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> block_log::read_raw_block_data_by_num(uint32_t block_num) const
  {
    block_log_artifacts::artifacts_t this_block_artifacts = my->_artifacts->read_block_artifacts(block_num);

    const uint64_t block_start_pos = this_block_artifacts.block_log_file_pos;
    const uint64_t serialized_data_size = this_block_artifacts.block_serialized_data_size;

    std::unique_ptr<char[]> serialized_data(new char[serialized_data_size]);
    size_t total_read = detail::block_log_impl::pread_with_retry(my->block_log_fd, serialized_data.get(), serialized_data_size, block_start_pos);
    FC_ASSERT(total_read == serialized_data_size);

    return std::make_tuple(std::move(serialized_data), serialized_data_size, std::move(this_block_artifacts));
  }

  // threading guarantees:
  // - this function may only be called by one thread at a time
  // - It is safe to call `append` while any number of other threads 
  //   are reading the block log.
  // There is no real use-case for multiple writers so it's not worth
  // adding a lock to allow it.
  uint64_t block_log::append(const std::shared_ptr<full_block_type>& full_block)
  {
    try
    {
      uint64_t block_start_pos;

      if (my->compression_enabled)
      {
        const compressed_block_data& compressed_block = full_block->get_compressed_block();
        block_start_pos = append_raw(full_block->get_block_num(),
                                     compressed_block.compressed_bytes.get(), compressed_block.compressed_size, compressed_block.compression_attributes,
                                     full_block->get_block_id());
      }
      else // compression not enabled
      {
        const uncompressed_block_data& uncompressed_block = full_block->get_uncompressed_block();
        block_start_pos = append_raw(full_block->get_block_num(),
                                     uncompressed_block.raw_bytes.get(), uncompressed_block.raw_size, {block_flags::uncompressed},
                                     full_block->get_block_id());
      }

      // update our cached head block
      std::atomic_store(&my->head, full_block);

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

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log::decompress_raw_block(const char* raw_block_data, size_t raw_block_size, block_attributes_t attributes)
  {
    try
    {
      switch (attributes.flags)
      {
      case block_log::block_flags::zstd:
        return block_log::decompress_block_zstd(raw_block_data, raw_block_size, attributes.dictionary_number);
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

  std::shared_ptr<full_block_type> block_log::read_block_by_num( uint32_t block_num )const
  {
    try
    {
      // first, check if it's the current head block; if so, we can just return it.  If the
      // block number is less than than the current head, it's guaranteed to have been fully
      // written to the log+index
      std::shared_ptr<full_block_type> head_block = my->head;
      /// \warning ignore block 0 which is invalid, but old API also returned empty result for it (instead of assert).
      if (block_num == 0 || !head_block || block_num > head_block->get_block_num())
        return std::shared_ptr<full_block_type>();
      if (block_num == head_block->get_block_num())
        return head_block;

      // if we're still here, we know that it's in the block log, and the block after it is also
      // in the block log (which means we can determine its size)
      std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> raw_block_data = read_raw_block_data_by_num(block_num);
      block_log_artifacts::artifacts_t artifacts = std::get<2>(std::move(raw_block_data));

      return artifacts.attributes.flags == block_flags::uncompressed ? 
        full_block_type::create_from_uncompressed_block_data(std::get<0>(std::move(raw_block_data)), std::get<1>(raw_block_data), artifacts.block_id) :
        full_block_type::create_from_compressed_block_data(std::get<0>(std::move(raw_block_data)), std::get<1>(raw_block_data), artifacts.attributes, artifacts.block_id);
    }
    FC_CAPTURE_LOG_AND_RETHROW((block_num))
  }

  std::shared_ptr<full_block_type> block_log::read_block_by_offset(uint64_t offset, size_t size, block_attributes_t attributes) const
  {
    std::unique_ptr<char[]> serialized_data(new char[size]);
    size_t total_read = detail::block_log_impl::pread_with_retry(my->block_log_fd, serialized_data.get(), size, offset);
    FC_ASSERT(total_read == size);

    return attributes.flags == block_flags::uncompressed ? 
      full_block_type::create_from_uncompressed_block_data(std::move(serialized_data), size) : 
      full_block_type::create_from_compressed_block_data(std::move(serialized_data), size, attributes);
  }

  std::vector<std::shared_ptr<full_block_type>> block_log::read_block_range_by_num(uint32_t first_block_num, uint32_t count) const
  {
    try
    {
      std::vector<std::shared_ptr<full_block_type>> result;

      uint32_t last_block_num = first_block_num + count - 1;

      // first, check if the last block we want is the current head block; if so, we can 
      // will use it and then load the previous blocks from the block log
      std::shared_ptr<full_block_type> head_block = my->head;
      if (!head_block || first_block_num > head_block->get_block_num())
        return result; // the caller is asking for blocks after the head block, we don't have them

      // if that head block will be our last block, we want it at the end of our vector,
      // so we'll tack it on at the bottom of this function
      bool last_block_is_head_block = last_block_num == head_block->get_block_num();
      uint32_t last_block_num_from_disk = last_block_is_head_block ? last_block_num - 1 : last_block_num;

      if (first_block_num <= last_block_num_from_disk)
      {
        result.reserve(count);

        // then we need to read blocks from the disk
        uint32_t number_of_blocks_to_read = last_block_num_from_disk - first_block_num + 1;

        size_t size_of_all_blocks = 0;
        auto plural_of_block_artifacts = my->_artifacts->read_block_artifacts(first_block_num, number_of_blocks_to_read, &size_of_all_blocks);

        uint64_t first_block_offset = plural_of_block_artifacts.front().block_log_file_pos;

        // then read all the blocks in one go
        idump((size_of_all_blocks));
        std::unique_ptr<char[]> block_data(new char[size_of_all_blocks]);
        detail::block_log_impl::pread_with_retry(my->block_log_fd, block_data.get(), size_of_all_blocks, first_block_offset);

        // now deserialize the blocks
        for (const block_log_artifacts::artifacts_t& block_artifacts : plural_of_block_artifacts)
        {
          // full_block_type expects to take ownership of a unique_ptr for the memory, so create one
          std::unique_ptr<char[]> compressed_block_data(new char[block_artifacts.block_serialized_data_size]);
          memcpy(compressed_block_data.get(), block_data.get() + block_artifacts.block_log_file_pos - first_block_offset, 
                 block_artifacts.block_serialized_data_size);
          if (block_artifacts.attributes.flags == block_flags::uncompressed)
            result.push_back(full_block_type::create_from_uncompressed_block_data(std::move(compressed_block_data), 
                                                                                  block_artifacts.block_serialized_data_size, 
                                                                                  block_artifacts.block_id));
          else
            result.push_back(full_block_type::create_from_compressed_block_data(std::move(compressed_block_data), 
                                                                                block_artifacts.block_serialized_data_size, 
                                                                                block_artifacts.attributes, block_artifacts.block_id));
        }
      }

      if (last_block_is_head_block)
        result.push_back(head_block);
      return result;
    }
    FC_CAPTURE_LOG_AND_RETHROW((first_block_num)(count))
  }

  std::tuple<std::unique_ptr<char[]>, size_t, block_log::block_attributes_t> block_log::read_raw_head_block() const
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
    std::tie(head_block_offset, attributes) = detail::split_block_start_pos_with_flags(head_block_offset_with_flags);
    size_t raw_data_size = block_log_size - head_block_offset - sizeof(head_block_offset);

    FC_ASSERT(
      raw_data_size <= HIVE_MAX_BLOCK_SIZE,
      "block log file is corrupted, head block has invalid size: ${raw_data_size} bytes",
      (raw_data_size)
    );

    FC_ASSERT(
      (size_t)(block_log_size) > (head_block_offset - sizeof(head_block_offset)),
      "block log file is corrupted, head block offset is greater than file size; block_log_size=${block_log_size}, head_block_offset=${head_block_offset}",
      (block_log_size)(head_block_offset)
    );

    std::unique_ptr<char[]> raw_data(new char[raw_data_size]);
    auto total_read = detail::block_log_impl::pread_with_retry(my->block_log_fd, raw_data.get(), raw_data_size, head_block_offset);
    FC_ASSERT(total_read == raw_data_size);

    return std::make_tuple(std::move(raw_data), raw_data_size, attributes);
  }

  // not thread safe, but it's only called when opening the block log, we can assume we're the only thread accessing it
  std::shared_ptr<full_block_type> block_log::read_head()const
  {
    try
    {
      std::tuple<std::unique_ptr<char[]>, size_t, block_log::block_attributes_t> raw_block_data = read_raw_head_block();
      block_log::block_attributes_t attributes = std::get<2>(raw_block_data);

      return attributes.flags == block_flags::uncompressed ? 
        full_block_type::create_from_uncompressed_block_data(std::get<0>(std::move(raw_block_data)), std::get<1>(raw_block_data)) :
        full_block_type::create_from_compressed_block_data(std::get<0>(std::move(raw_block_data)), std::get<1>(raw_block_data), attributes);
    }
    FC_LOG_AND_RETHROW()
  }

  std::shared_ptr<full_block_type> block_log::head() const
  {
    return my->head;
  }

  void block_log::set_compression(bool enabled)
  {
    my->compression_enabled = enabled;
  }

  void block_log::set_compression_level(int level)
  {
    my->zstd_level = level;
  }

  std::tuple<std::unique_ptr<char[]>, size_t> compress_block_zstd_helper(const char* uncompressed_block_data, 
                                                                         size_t uncompressed_block_size,
                                                                         std::optional<uint8_t> dictionary_number,
                                                                         ZSTD_CCtx* compression_context,
                                                                         fc::optional<int> compression_level)
  {
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

    size_t zstd_compressed_size = ZSTD_compress2(compression_context, 
                                                 zstd_compressed_data.get(), zstd_max_size,
                                                 uncompressed_block_data, uncompressed_block_size);

    if (ZSTD_isError(zstd_compressed_size))
      FC_THROW("Error compressing block with zstd");


    return std::make_tuple(std::move(zstd_compressed_data), zstd_compressed_size);
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log::compress_block_zstd(const char* uncompressed_block_data, 
                                                                                          size_t uncompressed_block_size,
                                                                                          std::optional<uint8_t> dictionary_number,
                                                                                          fc::optional<int> compression_level,
                                                                                          fc::optional<ZSTD_CCtx*> compression_context_for_reuse)
  {
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
  }

  std::tuple<std::unique_ptr<char[]>, size_t> decompress_block_zstd_helper(const char* compressed_block_data,
                                                                           size_t compressed_block_size,
                                                                           std::optional<uint8_t> dictionary_number,
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
                                                                                            std::optional<uint8_t> dictionary_number,
                                                                                            fc::optional<ZSTD_DCtx*> decompression_context_for_reuse)
  {
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

  }

  void block_log::for_each_block_position(block_info_processor_t processor) const
  {
    FC_ASSERT(is_open(), "Open block log first !");

    if (my->block_log_size == 0)
      return; /// Nothing to do for empty block log.

    uint64_t time_begin = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    std::shared_ptr<full_block_type> head_block = my->head;

    FC_ASSERT(head_block != nullptr);

    uint32_t head_block_num = head_block->get_block_num();

    // memory map for block log
    char* block_log_ptr = (char*)mmap(0, my->block_log_size, PROT_READ, MAP_SHARED, my->block_log_fd, 0);
    if (block_log_ptr == (char*)-1)
      FC_THROW("Failed to mmap block log file: ${error}", ("error", strerror(errno)));
    if (madvise(block_log_ptr, my->block_log_size, MADV_WILLNEED) == -1)
      wlog("madvise failed: ${error}", ("error", strerror(errno)));

    // now walk backwards through the block log reading the starting positions of the blocks
    uint64_t block_pos = my->block_log_size - sizeof(uint64_t);
    
    ilog("Attempting to walk over block position list starting from block: ${b}...", ("b", head_block_num));

    for (uint32_t block_num = head_block_num; block_num >= 1; --block_num)
    {
      // read the file offset of the start of the block from the block log
      uint64_t higher_block_pos = block_pos;
      // read next block pos offset from the block log
      uint64_t block_pos_with_flags = 0;
      memcpy(&block_pos_with_flags, block_log_ptr + block_pos, sizeof(block_pos_with_flags));

      auto block_pos_info = detail::split_block_start_pos_with_flags(block_pos_with_flags);
      block_pos = block_pos_info.first;
      if (higher_block_pos <= block_pos) //this is a sanity check on index values stored in the block log
        FC_THROW("bad block offset at block ${block_num} because higher block pos: ${higher_block_pos} <= lower block pos: ${block_pos}",
          ("block_num", block_num)("higher_block_pos", higher_block_pos)("block_pos", block_pos));

      uint32_t block_serialized_data_size = higher_block_pos - block_pos;

      if (processor(block_num, block_serialized_data_size, block_pos, block_pos_info.second) == false)
      {
        ilog("Stopping block position list walk on caller request... Last processed block: ${b}", ("b", block_num));
        break;
      }

      /// Move to the offset of previous block
      block_pos -= sizeof(uint64_t);
    }

    if (munmap(block_log_ptr, my->block_log_size) == -1)
      elog("error unmapping block_log: ${error}", ("error", strerror(errno)));

    uint64_t time_end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto elapsed_time = time_end - time_begin;

    ilog("Block position list walk finished in time: ${et} ms.", ("et", elapsed_time/1000));
  }

  // calls your callback with every block, in reverse order
  void block_log::for_each_block(block_processor_t processor) const
  {
    FC_ASSERT(is_open(), "Open block log first!");

    if (my->block_log_size == 0)
      return; /// Nothing to do for empty block log.

    fc::time_point iteration_begin = fc::time_point::now();
    std::shared_ptr<full_block_type> head_block = my->head;
    FC_ASSERT(head_block);
    
    uint32_t head_block_num = head_block->get_block_num();
    
    // memory map for block log
    char* block_log_ptr = (char*)mmap(0, my->block_log_size, PROT_READ, MAP_SHARED, my->block_log_fd, 0);
    if (block_log_ptr == (char*)-1)
      FC_THROW("Failed to mmap block log file: ${error}", ("error", strerror(errno)));
    if (madvise(block_log_ptr, my->block_log_size, MADV_WILLNEED) == -1)
      wlog("madvise failed: ${error}", ("error", strerror(errno)));
    
    // now walk backwards through the block log reading the starting positions of the blocks
    uint64_t block_pos = my->block_log_size - sizeof(uint64_t);

    ilog("Walking over block log starting from block: ${head_block_num}...", (head_block_num));
    
    for (uint32_t block_num = head_block_num; block_num >= 1; --block_num)
    {
      // read the file offset of the start of the block from the block log
      uint64_t higher_block_pos = block_pos;
      // read next block pos offset from the block log
      uint64_t block_pos_with_flags = *(uint64_t*)(block_log_ptr + block_pos);
    
      block_attributes_t attributes;
      std::tie(block_pos, attributes) = detail::split_block_start_pos_with_flags(block_pos_with_flags);
    
      if (higher_block_pos <= block_pos) //this is a sanity check on index values stored in the block log
        FC_THROW("bad block offset at block ${block_num} because higher block pos: ${higher_block_pos} <= lower block pos: ${block_pos}",
                 (block_num)(higher_block_pos)(block_pos));
    
      uint32_t block_serialized_data_size = higher_block_pos - block_pos;
    
      std::unique_ptr<char[]> serialized_data(new char[block_serialized_data_size]);
      memcpy(serialized_data.get(), block_log_ptr + block_pos, block_serialized_data_size);
      std::shared_ptr<full_block_type> full_block = attributes.flags == block_flags::uncompressed ? 
          full_block_type::create_from_uncompressed_block_data(std::move(serialized_data), block_serialized_data_size) : 
          full_block_type::create_from_compressed_block_data(std::move(serialized_data), block_serialized_data_size, attributes);
    
      if (!processor(block_num, full_block, block_pos, attributes))
      {
        ilog("Stopping block position list walk on caller request... Last processed block: ${block_num}", (block_num));
        break;
      }
    
      /// Move to the offset of previous block
      block_pos -= sizeof(uint64_t);
    }
    
    if (munmap(block_log_ptr, my->block_log_size) == -1)
      elog("error unmapping block_log: ${error}", ("error", strerror(errno)));
    
    fc::time_point iteration_end = fc::time_point::now();
    fc::microseconds iteration_duration = iteration_end - iteration_begin;

    ilog("Block log walk finished in time: ${iteration_duration} s.", ("iteration_duration", iteration_duration.count() / 1000000));
  }

} } // hive::chain

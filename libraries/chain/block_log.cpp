#include <hive/chain/block_log.hpp>
#include <hive/protocol/config.hpp>

#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/chain/block_log_compression.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/detail/block_attributes.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <queue>
#include <fstream>
#include <fc/io/raw.hpp>
#include <fc/thread/thread.hpp>

#include <appbase/application.hpp>

#include <hive/utilities/io_primitives.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/lock_options.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/filesystem.hpp>

#include <unistd.h>

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

  const std::string block_log_file_name_info::_legacy_file_name = "block_log";
  const std::string block_log_file_name_info::_split_file_name_core = "block_log_part";
  const std::string block_log_file_name_info::_artifacts_extension = ".artifacts";

  uint32_t block_log_file_name_info::get_first_block_num_for_file_name( const fc::path& block_log_path )
  {
    std::string stem(block_log_path.stem().string());
    if(stem == _legacy_file_name)
    {
      return 1; // First block number in legacy single file block log is obviously 1.
    }

    uint32_t part_number = is_part_file(block_log_path);
    FC_ASSERT(part_number > 0);

    // First block number in split block log part file is 1 plus all blocks in previous files.
    // Note that part files are numbered beginning with 0001.
    return 1 + ((part_number -1) * BLOCKS_IN_SPLIT_BLOCK_LOG_FILE);
  }

  std::string block_log_file_name_info::get_nth_part_file_name( uint32_t part_number )
  {
    // "^block_log_part\\.\\d{4}$"
    size_t padding = _split_file_name_extension_length;
    std::string part_number_str = std::to_string( part_number );
    std::string result = _split_file_name_core + "." + 
      std::string(padding - std::min(padding, part_number_str.length()), '0') + part_number_str;
    return result;
  }

  uint32_t block_log_file_name_info::is_part_file( const fc::path& file )
  {
    std::string extension(file.extension().string());
    if(extension.empty())
      return 0;

    // Note that fc::path::extension() contains leading dot.
    extension = extension.substr(1);

    // "^block_log_part\\.\\d{4}$"
    if (file.stem().string() != _split_file_name_core ||
        extension.size() != _split_file_name_extension_length ||
        not std::all_of( extension.begin(), extension.end(), [](unsigned char ch){ return std::isdigit(ch); } ))
      return 0;

    return std::stoul(extension);
  }

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
        boost::interprocess::file_lock _flock;

        static void write_with_retry(int filedes, const void* buf, size_t nbyte);
        static void pwrite_with_retry(int filedes, const void* buf, size_t nbyte, off_t offset);
        static size_t pread_with_retry(int filedes, void* buf, size_t nbyte, off_t offset);

        // only accessed when appending a block, doesn't need locking
        ssize_t block_log_size;

        bool compression_enabled = true;
        bool auto_fixing_enabled = true;

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

  block_log::block_log( appbase::application& app ) : my( new detail::block_log_impl() ), theApp( app )
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
    struct stat get_file_stats(int fd)
    {
      struct stat file_stats;
      if (fstat(fd, &file_stats) == -1)
        FC_THROW("Error getting size of file: ${error}", ("error", strerror(errno)));
      return file_stats;
    }
  }

  void block_log::open(const fc::path& file, hive::chain::blockchain_worker_thread_pool& thread_pool, bool read_only /* = false */, bool write_fallback /*= false*/, bool auto_open_artifacts /*= true*/ )
  {
      FC_ASSERT(!fc::is_directory(file), "${file} should point to block_log file, not directory", (file));
      close();

      my->block_file = file;

      {
        const fc::path parent_path = my->block_file.parent_path();
        if (!fc::exists(parent_path) && !parent_path.empty())
          boost::filesystem::create_directories( my->block_file.parent_path().generic_string() );
      }

      std::string file_str = my->block_file.generic_string();

      int flags = O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC;
      if (read_only)
      {
        flags = O_RDONLY | O_CLOEXEC;
        if(not fc::exists(file) && write_fallback)
        {
          int temp_flags = flags | O_CREAT;
          int temp_fd = ::open(file_str.c_str(), temp_flags, 0644);
          if (temp_fd == -1)
            FC_THROW("Error creating block log file ${filename}: ${error}",
              ("filename", my->block_file)("error", strerror(errno)));

          ::close(temp_fd);
        }
      }

      ilog("Opening blocklog ${blocklog_filename} in ${mode} mode ...",
        ("blocklog_filename",file_str.c_str())("mode", read_only ? "read only" : "read write"));
      my->block_log_fd = ::open(file_str.c_str(), flags, 0644);
      if (my->block_log_fd == -1)
        FC_THROW("Error opening block log file ${filename} in ${mode} mode: ${error}",
          ("filename", my->block_file)("error", strerror(errno))("mode", read_only ? "read only" : "read write"));

      struct stat block_log_stats = get_file_stats(my->block_log_fd);
      if( (block_log_stats.st_mode & 0200) == 0 )
      {
        wlog( "Block log file ${file_cstr} is read-only. Skipping advisory file lock initiation.", ("file_cstr", file_str.c_str()) );
      }
      else
      {
        my->_flock = boost::interprocess::file_lock(file_str.c_str());

        if (read_only)
        {
          if (!my->_flock.try_lock_sharable())
            FC_THROW("Unable to get sharable access to block_log file: ${file_cstr} (some other process opened block_log in RW mode probably)", ("file_cstr", file_str.c_str()));
        }
        else
        {
          if (!my->_flock.try_lock())
            FC_THROW("Unable to get read & write access to block_log file: ${file_cstr} (some other process opened block_log probably)", ("file_cstr", file_str.c_str()));
        }
      }

      my->block_log_size = block_log_stats.st_size;

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
      if (my->block_log_size)
      {
        sanity_check(read_only);
        idump((my->block_log_size));
        std::atomic_store(&my->head, read_head());
      }

      if (auto_open_artifacts)
          my->_artifacts = block_log_artifacts::open(file, *this, read_only, write_fallback, false, theApp, thread_pool);
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

  fc::path block_log::get_log_file() const
  {
    return my->block_file;
  }

  fc::path block_log::get_artifacts_file() const
  {
    return my->_artifacts ? my->_artifacts->get_artifacts_file() : fc::path();
  }

  block_id_type get_block_id(const char* raw_block_data, size_t raw_block_size, const block_attributes_t& attributes)
  {
    std::unique_ptr<char[]> compressed_block_data(new char[raw_block_size]);
    memcpy(compressed_block_data.get(), raw_block_data, raw_block_size);

    std::shared_ptr<full_block_type> full_block;
    if (attributes.flags == block_flags::uncompressed)
      full_block = full_block_type::create_from_uncompressed_block_data(std::move(compressed_block_data), raw_block_size);
    else
      full_block = full_block_type::create_from_compressed_block_data(std::move(compressed_block_data), raw_block_size, attributes);

    return full_block->get_block_id();
  }

  uint64_t block_log::append_raw(uint32_t block_num, const char* raw_block_data, size_t raw_block_size, const block_attributes_t& attributes, const bool is_at_live_sync)
  {
    auto block_id = get_block_id(raw_block_data, raw_block_size, attributes);
    return append_raw(block_num, raw_block_data, raw_block_size, attributes, block_id, is_at_live_sync);
  }

  uint64_t block_log::append_raw(uint32_t block_num, const char* raw_block_data, size_t raw_block_size, const block_attributes_t& attributes, const block_id_type& block_id, const bool is_at_live_sync)
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

      my->_artifacts->store_block_artifacts(block_num, block_start_pos, attributes, block_id, is_at_live_sync);

      return block_start_pos;
    }
    FC_CAPTURE_LOG_AND_RETHROW((block_num)(block_start_pos)(block_start_pos_with_flags)(attributes))
  }

  void block_log::multi_append_raw(uint32_t first_block_num,
    std::tuple<std::unique_ptr<char[]>, block_log_artifacts::artifact_container_t, uint64_t>& data)
  {
    size_t buffer_offset = 0;
    block_log_artifacts::artifact_data_container_t artifacts_data;
    // We will override file position offsets bundled with raw block data
    // with actual offsets of this file.
    std::unique_ptr<char[]>& the_buffer = std::get<0>(data);
    const block_log_artifacts::artifact_container_t& plural_of_artifacts = std::get<1>(data);
    uint32_t block_num = first_block_num;
    for(const block_log_artifacts::artifacts_t& artifacts : plural_of_artifacts)
    {
      size_t raw_block_size = artifacts.block_serialized_data_size;
      uint64_t block_start_pos = my->block_log_size;
      uint64_t block_start_pos_with_flags = detail::combine_block_start_pos_with_flags(block_start_pos, artifacts.attributes);
      *(uint64_t*)(the_buffer.get() + buffer_offset + raw_block_size) = block_start_pos_with_flags;

      size_t block_size_including_start_pos = raw_block_size + sizeof(uint64_t);
      buffer_offset += block_size_including_start_pos;
      my->block_log_size += block_size_including_start_pos;

      block_id_type block_id = artifacts.block_id;
      if(block_id == block_id_type())
      {
        block_id = get_block_id(the_buffer.get() + buffer_offset, raw_block_size, artifacts.attributes);
        ilog("Block id is ${block_id}", (block_id));
      }
      artifacts_data.emplace_back(block_num, block_start_pos, artifacts.attributes, block_id);
      ++block_num;
    }

    detail::block_log_impl::write_with_retry(my->block_log_fd, the_buffer.get(), buffer_offset);
    //the_buffer.reset();
    my->_artifacts->store_block_artifacts(artifacts_data, false/*is_at_live_sync*/);
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
  uint64_t block_log::append(const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync)
  {
    try
    {
      uint64_t block_start_pos;

      if (my->compression_enabled)
      {
        const compressed_block_data& compressed_block = full_block->get_compressed_block();
        block_start_pos = append_raw(full_block->get_block_num(),
                                     compressed_block.compressed_bytes.get(), compressed_block.compressed_size, compressed_block.compression_attributes,
                                     full_block->get_block_id(), is_at_live_sync);
      }
      else // compression not enabled
      {
        const uncompressed_block_data& uncompressed_block = full_block->get_uncompressed_block();
        block_start_pos = append_raw(full_block->get_block_num(),
                                     uncompressed_block.raw_bytes.get(), uncompressed_block.raw_size, {block_flags::uncompressed},
                                     full_block->get_block_id(), is_at_live_sync);
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

  hive::protocol::block_id_type block_log::read_block_id_by_num(uint32_t block_num) const
  {
    std::shared_ptr<full_block_type> head_block = my->head;

    /// \warning ignore block 0 which is invalid, but old API also returned empty result for it (instead of assert).
    if(block_num == 0 || !head_block || block_num > head_block->get_block_num())
      return hive::protocol::block_id_type();

    if(block_num == head_block->get_block_num())
      return head_block->get_block_id();

    block_log_artifacts::artifacts_t block_artifacts = my->_artifacts->read_block_artifacts(block_num);

//    auto block = read_block_by_num(block_num);
//    FC_ASSERT(block->get_block_id() != block_artifacts.block_id);

    return block_artifacts.block_id;
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

  std::tuple<std::unique_ptr<char[]>, block_log_artifacts::artifact_container_t, uint64_t>
  block_log::multi_read_raw_block_data(uint32_t first_block_num, uint32_t last_block_num_from_disk) const
  {
    // then we need to read blocks from the disk
    uint32_t number_of_blocks_to_read = last_block_num_from_disk - first_block_num + 1;

    size_t size_of_all_blocks = 0;
    auto plural_of_block_artifacts = my->_artifacts->read_block_artifacts(first_block_num, number_of_blocks_to_read, &size_of_all_blocks);

    uint64_t first_block_offset = plural_of_block_artifacts.front().block_log_file_pos;

    // then read all the blocks in one go
    idump((size_of_all_blocks));
    std::unique_ptr<char[]> block_data(new char[size_of_all_blocks]);
    detail::block_log_impl::pread_with_retry(my->block_log_fd, block_data.get(), size_of_all_blocks, first_block_offset);
    
    return std::make_tuple(std::move(block_data), std::move(plural_of_block_artifacts), first_block_offset);
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

        auto read_result = multi_read_raw_block_data(first_block_num, last_block_num_from_disk);
        std::unique_ptr<char[]> block_data(std::move(std::get<0>(read_result)));
        auto plural_of_block_artifacts(std::move(std::get<1>(read_result)));
        uint64_t first_block_offset = std::get<2>(read_result);

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
    ssize_t block_log_size = get_file_stats(my->block_log_fd).st_size;

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

    FC_ASSERT(raw_data_size <= HIVE_MAX_BLOCK_SIZE, "block log file is corrupted, head block has invalid size: ${raw_data_size} bytes", (raw_data_size));

    FC_ASSERT((head_block_offset < sizeof(head_block_offset)) /*e.g. single empty block in log*/ ||
              ((size_t)(block_log_size) > (head_block_offset - sizeof(head_block_offset))),
              "block log file is corrupted, head block offset is greater than file size; block_log_size=${block_log_size}, head_block_offset=${head_block_offset}",
              (block_log_size)(head_block_offset));

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

  void block_log::open_and_init( const fc::path& file, bool read_only, bool enable_compression,
    int compression_level, bool enable_block_log_auto_fixing, hive::chain::blockchain_worker_thread_pool& thread_pool )
  {
    my->auto_fixing_enabled = enable_block_log_auto_fixing;
    open( file, thread_pool, read_only, true /*write_fallback*/ );
    my->compression_enabled = enable_compression;
    my->zstd_level = compression_level;
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

  void block_log::read_blocks_data_for_artifacts_generation(artifacts_generation_processor processor, const uint32_t target_block_number, const uint32_t starting_block_number,
                                                            const fc::optional<uint64_t> starting_block_position) const
  {
    FC_ASSERT(target_block_number < starting_block_number);
    FC_ASSERT(target_block_number != 0);
    FC_ASSERT(is_open(), "Open block log first!");
    FC_ASSERT(my->block_log_size, "Cannot process blocks from empty block_log.");

    const std::shared_ptr<full_block_type> head_block = my->head;
    FC_ASSERT(head_block);

    const uint32_t head_block_num = head_block->get_block_num();
    FC_ASSERT(starting_block_number <= head_block_num);

    uint64_t block_position = 0;
    uint32_t current_block_num = starting_block_number;

    const bool starting_from_head_block = starting_block_number == head_block_num;

    if (starting_from_head_block)
      block_position = my->block_log_size;
    else
    {
      FC_ASSERT(starting_block_position, "if starting_block_number is not head block, starting_block_position is mandatory");
      block_position = *starting_block_position;
      --current_block_num;
    }

    // memory map for block log
    char* block_log_ptr = (char*)mmap(0, my->block_log_size, PROT_READ, MAP_SHARED, my->block_log_fd, 0);
    if (block_log_ptr == (char*)-1)
      FC_THROW("Failed to mmap block log file: ${error}", ("error", strerror(errno)));
    if (madvise(block_log_ptr, my->block_log_size, MADV_WILLNEED) == -1)
      wlog("madvise failed: ${error}", ("error", strerror(errno)));

    ilog("Processing blocks in reverse order for artifact file from block: ${starting_block_number} (position: ${block_position}, is head: ${starting_from_head_block} ) to ${target_block_number} ...",
      (starting_block_number)(block_position)(starting_from_head_block)(target_block_number));
    
    block_position -= sizeof(uint64_t);
    const fc::time_point start_time = fc::time_point::now();

    while (current_block_num >= target_block_number)
    {
      // read the file offset of the start of the block from the block log
      uint64_t higher_block_position = block_position;
      // read next block pos offset from the block log
      uint64_t block_position_with_flags = *(uint64_t*)(block_log_ptr + block_position);
      block_attributes_t attributes;
      std::tie(block_position, attributes) = detail::split_block_start_pos_with_flags(block_position_with_flags);
    
      if (higher_block_position <= block_position) //this is a sanity check on index values stored in the block log
        FC_THROW("bad block offset at block ${current_block_num} because higher block pos: ${higher_block_pos} <= lower block pos: ${block_position}",
                 (current_block_num)(higher_block_position)(block_position));
    
      uint32_t block_serialized_data_size = higher_block_position - block_position;
    
      std::unique_ptr<char[]> serialized_data(new char[block_serialized_data_size]);
      memcpy(serialized_data.get(), block_log_ptr + block_position, block_serialized_data_size);
      std::shared_ptr<full_block_type> full_block = attributes.flags == block_flags::uncompressed ? 
          full_block_type::create_from_uncompressed_block_data(std::move(serialized_data), block_serialized_data_size) : 
          full_block_type::create_from_compressed_block_data(std::move(serialized_data), block_serialized_data_size, attributes);

      if (!processor(full_block, block_position, current_block_num, attributes))
      {
        ilog("Block log reading for artifacts stopped on caller request. Last read block: ${current_block_num}", (current_block_num));
        break;
      }
    
      /// Move to the offset of previous block
      block_position -= sizeof(uint64_t);
      --current_block_num;
    }
    
    if (munmap(block_log_ptr, my->block_log_size) == -1)
      elog("error unmapping block_log: ${error}", ("error", strerror(errno)));
    
    const fc::time_point end_time = fc::time_point::now();
    const fc::microseconds iteration_duration = end_time - start_time;
    ilog("Block log reading for artifacts finished in time: ${iteration_duration} s.", ("iteration_duration", iteration_duration.count() / 1000000));
  }

  void block_log::for_each_block(uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor,
                                 block_log::for_each_purpose purpose,
                                 hive::chain::blockchain_worker_thread_pool& thread_pool) const
  {
    constexpr uint32_t max_blocks_to_prefetch = 1000;

    std::queue<std::shared_ptr<full_block_type>> block_queue;
    bool stop_requested = false;
    std::mutex block_queue_mutex;
    std::condition_variable block_queue_condition;

    hive::chain::blockchain_worker_thread_pool::data_source_type worker_thread_processing;
    switch (purpose)
    {
      case for_each_purpose::replay:
        worker_thread_processing = hive::chain::blockchain_worker_thread_pool::data_source_type::block_log_for_replay;
        break;
      case for_each_purpose::decompressing:
        worker_thread_processing = hive::chain::blockchain_worker_thread_pool::data_source_type::block_log_for_decompressing;
        break;
      default:
        FC_THROW("unknown purpose");
    }

    std::thread queue_filler_thread([&, this]() {
      fc::set_thread_name("for_each_io"); // tells the OS the thread's name
      fc::thread::current().set_name("for_each_io"); // tells fc the thread's name for logging
      for (uint32_t block_number = starting_block_number; block_number <= ending_block_number; ++block_number)
      {
        std::shared_ptr<full_block_type> full_block = read_block_by_num(block_number);
        {
          std::unique_lock<std::mutex> lock(block_queue_mutex);
          while (block_queue.size() >= max_blocks_to_prefetch && !stop_requested)
            block_queue_condition.wait(lock);
          if (stop_requested)
          {
            ilog("Leaving the queue thread");
            return;
          }
          block_queue.push(full_block);
          block_queue_condition.notify_one();
        }
        thread_pool.enqueue_work(full_block, worker_thread_processing);
      }
    
      ilog("Exiting the queue thread");
    });

    for (uint32_t block_number = starting_block_number; block_number <= ending_block_number; ++block_number)
    {
      std::shared_ptr<full_block_type> full_block;
      {
        std::unique_lock<std::mutex> lock(block_queue_mutex);
        while (block_queue.empty() && !stop_requested)
          block_queue_condition.wait(lock);

        if(!stop_requested)
        { 
          full_block = block_queue.front();
          block_queue.pop();
        }

        block_queue_condition.notify_one();
      }

      try
      {
        if(!stop_requested)
          stop_requested = !processor(full_block);

        if (stop_requested)
        {
          ilog("Attempting to break a block processing loop and request block queue stop.");
          std::unique_lock<std::mutex> lock(block_queue_mutex);
          block_queue_condition.notify_one();
          ilog("Block queue stop requested.");
          break;
        }
      }
      FC_CAPTURE_CALL_LOG_AND_RETHROW(([&, this]()
        {
          thread_pool.shutdown();

          {
            std::unique_lock<std::mutex> lock(block_queue_mutex);
            stop_requested = true;
            block_queue_condition.notify_one();
          }

          ilog("Attempting to join queue_filler_thread...");
          queue_filler_thread.join();
          ilog("queue_filler_thread joined.");
        }), ());
    }

    ilog("Attempting to join queue_filler_thread...");
    queue_filler_thread.join();
    ilog("queue_filler_thread joined.");
  }

  void block_log::truncate(uint32_t new_head_block_num)
  {
    dlog("truncating block log to ${new_head_block_num} blocks", (new_head_block_num));
    std::shared_ptr<full_block_type> head_block = my->head;
    const uint32_t current_head_block_num = head_block ? head_block->get_block_num() : 0;

    if (new_head_block_num > current_head_block_num)
      FC_THROW("Unable to truncate block log to ${new_head_block_num}, it only has ${current_head_block_num} blocks in it", 
               (new_head_block_num)(current_head_block_num));
    if (current_head_block_num == new_head_block_num)
      return; // nothing to do

    // else, we're really being asked to shorten the block log
    block_log_artifacts::artifacts_t new_head_block_artifacts = my->_artifacts->read_block_artifacts(new_head_block_num);
    dlog("new head block starts at offset ${offset} and is ${bytes} bytes long", 
         ("offset", new_head_block_artifacts.block_log_file_pos)("bytes", new_head_block_artifacts.block_serialized_data_size));
    off_t final_block_log_size = new_head_block_artifacts.block_log_file_pos + new_head_block_artifacts.block_serialized_data_size + sizeof(uint64_t);;
    FC_ASSERT(ftruncate(my->block_log_fd, final_block_log_size) == 0, 
              "failed to truncate block log, ${error}", ("error", strerror(errno)));
    my->_artifacts->truncate(new_head_block_num);
    std::atomic_store(&my->head, read_head());
  }

  void block_log::sanity_check(const bool read_only)
  {
    // read the last int64 of the block log into `head_block_offset`,
    // that's the index of the start of the head block
    const ssize_t block_log_size = my->block_log_size;

    FC_ASSERT(block_log_size >= (ssize_t)sizeof(uint64_t));
    uint64_t head_block_offset_with_flags;
    detail::block_log_impl::pread_with_retry(my->block_log_fd, &head_block_offset_with_flags, sizeof(head_block_offset_with_flags), block_log_size - sizeof(head_block_offset_with_flags));
    uint64_t head_block_offset;

    block_attributes_t attributes;
    std::tie(head_block_offset, attributes) = detail::split_block_start_pos_with_flags(head_block_offset_with_flags);
    size_t raw_data_size = block_log_size - head_block_offset - sizeof(head_block_offset);
    if (read_only)
      FC_ASSERT(raw_data_size <= HIVE_MAX_BLOCK_SIZE, "block log file is corrupted, head block has invalid size: ${raw_data_size} bytes. Cannot modify block_log, opened in read_only mode.", (raw_data_size));
    else
    {
      if (my->auto_fixing_enabled && raw_data_size > HIVE_MAX_BLOCK_SIZE)
      {
        try
        {
          elog("block log file is corrupted, head block has invalid size: ${raw_data_size} bytes. block_log-auto-fixing enabled, trying to find place where head block should start.", (raw_data_size));

          // Reads the 8 bytes at the given location, and determines whether they "look like" the flags/offset byte that's written at the end of each block.
          // if it looked reasonable, returns the start of the block it would point at.
          auto is_data_at_file_position_a_plausible_offset_and_flags = [&my = my](const uint64_t offset_of_pos_and_flags_to_test) -> std::optional<uint64_t>
          {
            uint64_t block_offset_with_flags = 0;
            hive::utilities::perform_read(my->block_log_fd, (char*)&block_offset_with_flags, sizeof(block_offset_with_flags), offset_of_pos_and_flags_to_test, "read block offset");
            const auto [offset, flags] = hive::chain::detail::split_block_start_pos_with_flags(block_offset_with_flags);

            // check that the offset of the start of the block wouldn't mean that it's impossibly large
            const bool offset_is_plausible = offset < offset_of_pos_and_flags_to_test && offset >= offset_of_pos_and_flags_to_test - HIVE_MAX_BLOCK_SIZE;

            // check that no reserved flags are set, only "zstd/uncompressed" and "has dictionary" are permitted
            const bool flags_are_plausible = (block_offset_with_flags & 0x7e00000000000000ull) == 0;

            bool dictionary_is_plausible;
            // if the dictionary flag bit is set, verify that the dictionary number is one that we have.
            if (block_offset_with_flags & 0x0100000000000000ull)
              dictionary_is_plausible = *flags.dictionary_number <= hive::chain::get_last_available_zstd_compression_dictionary_number().value_or(0);
            // if the dictionary flag bit is not set, expect the dictionary number to be zeroed
            else
              dictionary_is_plausible = (block_offset_with_flags & 0x00ff000000000000ull) == 0;

            return offset_is_plausible && flags_are_plausible && dictionary_is_plausible ? offset : std::optional<uint64_t>();
          };

          uint64_t offset_of_pos_and_flags_to_test = block_log_size - sizeof(uint64_t);
          bool block_log_has_been_truncated = false;

          for (size_t i = 0; i < HIVE_MAX_BLOCK_SIZE; ++i)
          {
            std::optional<uint64_t> possible_start_of_block = is_data_at_file_position_a_plausible_offset_and_flags(offset_of_pos_and_flags_to_test);

            if (possible_start_of_block)
            {
              // double check if start of block is really found. Go backward for 10 blocks.
              bool verification_successfull = true;
              try
              {
                uint64_t block_position = *possible_start_of_block - sizeof(uint64_t);
                uint32_t expected_block_num = 0;

                for (uint8_t j = 0; j < 10; ++j)
                {
                  const uint64_t higher_block_position = block_position;
                  uint64_t block_position_with_flags = 0;
                  hive::utilities::perform_read(my->block_log_fd, (char*)&block_position_with_flags, sizeof(block_position_with_flags), block_position, "read block pos and flags");
                  block_attributes_t attributes;
                  std::tie(block_position, attributes) = detail::split_block_start_pos_with_flags(block_position_with_flags);

                  if (higher_block_position <= block_position)
                    FC_THROW("new block position: ${block_position} is higher then previous block position: ${higher_block_position}", (block_position)(higher_block_position));

                  const uint32_t block_serialized_data_size = higher_block_position - block_position;
                  const uint32_t created_block_num = read_block_by_offset(block_position, block_serialized_data_size, attributes)->get_block_num();

                  if (expected_block_num && created_block_num != expected_block_num)
                    FC_THROW("Created block number has other block number than expected.");

                  expected_block_num = created_block_num - 1;
                  block_position -= sizeof(uint64_t);
                  FC_ASSERT(block_position < *possible_start_of_block, "Should never happen, block_log has less than 10 blocks or is heavy corrupted.");
                }
              }
              catch (const fc::exception& e)
              {
                wlog("Tested possible start of block at pos: ${position} turns out to be incorrect. FC error occured: ${error}.", ("position", *possible_start_of_block)("error", e.to_detail_string()));
                verification_successfull = false;
              }
              catch( const std::exception& e )
              {
                wlog("Tested possible start of block at pos: ${position} turns out to be incorrect. std error occured: ${error}.", ("position", *possible_start_of_block)("error", e.what()));
                verification_successfull = false;
              }
              catch(...)
              {
                wlog("Tested possible start of block at pos: ${position} turns out to be incorrect. Unknown error occured.", ("position", *possible_start_of_block));
                verification_successfull = false;
              }

              if (verification_successfull)
              {
                block_log_has_been_truncated = true;
                const uint64_t new_block_log_size = offset_of_pos_and_flags_to_test + sizeof(uint64_t);
                wlog("Found end of last completed block in block_log. Truncating block_log to: ${new_block_log_size} bytes. Original block_log size: ${block_log_size}. Diff: ${diff}",
                    (new_block_log_size)(block_log_size)("diff", (block_log_size - new_block_log_size)));
                FC_ASSERT(ftruncate(my->block_log_fd, new_block_log_size) == 0, "failed to truncate block log, ${error}", ("error", strerror(errno)));
                wlog("block_log file has been truncated. Replay blockchain may be needed.");
                my->block_log_size = get_file_stats(my->block_log_fd).st_size;
                break;
              }
            }

            --offset_of_pos_and_flags_to_test;
            if (offset_of_pos_and_flags_to_test == 0 || offset_of_pos_and_flags_to_test > uint64_t(block_log_size))
              break;
          }

          if (!block_log_has_been_truncated)
            FC_THROW("Could not find last completed block in block_log. Autofixing failed, try manually repair block_log or delete it.");
        }
        FC_CAPTURE_AND_RETHROW()
      }
      else
        FC_ASSERT(raw_data_size <= HIVE_MAX_BLOCK_SIZE, "block log file is corrupted, head block has invalid size: ${raw_data_size} bytes. "
                                                        "Use --enable-block-log-auto-fixing, manually truncate block_log or delete block_log", (raw_data_size));
    }
  }

} } // hive::chain

#include <hive/chain/block_log_artifacts.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/full_block.hpp>

#include <hive/utilities/git_revision.hpp>
#include <hive/utilities/io_primitives.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <appbase/application.hpp>

#include <fc/bitutil.hpp>
#include <fc/thread/thread.hpp>

#include <boost/lockfree/queue.hpp>

#include <fcntl.h>

#include <chrono>
#include <future>
#include <map>
#include <thread>
#include <vector>
#include <queue>
#include <list>

namespace hive { namespace chain {

namespace { /// anonymous

using detail::block_flags;
using detail::block_attributes_t;

const uint16_t FORMAT_MAJOR = 1;
const uint16_t FORMAT_MINOR = 0;

#define HANDLE_IO(stmt, msg) \
{ \
  if(stmt == -1) { \
    wlog("Stmt: ${msg} failed with error: ${error}", ("error")(strerror(errno))); \
  } \
}

struct artifact_file_header
{
  artifact_file_header()
  {
    FC_ASSERT(strlen(hive::utilities::git_revision_sha) < sizeof(git_version));
    strcpy(git_version, hive::utilities::git_revision_sha);

    format_major_version = FORMAT_MAJOR; /// version info of storage format (to allow potential upgrades in the future)
    format_minor_version = FORMAT_MINOR;
    head_block_num = 0; /// number of newest (head) block the file holds informations for
    dirty_close = 0;
  }

  uint32_t head_block_num; /// number of newest (head) block the file holds informations for
  char git_version[41]; /// version of a tool which constructed given file
  uint8_t format_major_version; /// version info of storage format (to allow potential upgrades in the future)
  uint8_t format_minor_version;
  uint8_t dirty_close;
};

/**
 To store block_attributes we are using a fact that in the block log(and artifact file), the positions are stored as 64 - bit integers.
 We'll use the lower 48-bits as the actual position, and the upper 16 as flags that tell us how the block is stored
 hi    lo | hi     lo | hi      |        |        |        |        |      lo |
 c......d | < -dict-> | <-------------------- - position--------------------> |
  c = block_flags, one bit specifying the compression method, or uncompressed
  (this was briefly two bits when we were testing other compression methods)
  d = one bit, if 1 the block uses a custom compression dictionary
  dict = the number specifying the dictionary used to compress the block, if d = 1, otherwise undefined
  . = unused
*/
struct artifact_file_chunk
{
  union 
  {
    struct
    {
      uint64_t block_log_offset : 48;
      uint64_t dictionary_no : 8;
      uint64_t custom_dict_used : 1;
      uint64_t gap : 6;
      uint64_t is_compressed : 1;
    };

    uint64_t combined_pos_and_attrs;
  };

  /// Block id stripped by block_num (which is known at query time, so it can be supplemented to build full block_id)
  uint64_t stripped_block_id[2];

  block_log_artifacts::artifacts_t unpack_data(uint32_t block_num) const
  {
    auto unpacked_data = detail::split_block_start_pos_with_flags(combined_pos_and_attrs);

    FC_ASSERT(is_compressed == (unpacked_data.second.flags == block_flags::zstd));
    FC_ASSERT(custom_dict_used == (bool)unpacked_data.second.dictionary_number);
    FC_ASSERT(custom_dict_used == 0 || (dictionary_no == *unpacked_data.second.dictionary_number));
    FC_ASSERT(block_log_offset == unpacked_data.first);

    block_log_artifacts::artifacts_t artifacts;
  
    artifacts.attributes = unpacked_data.second;
    artifacts.block_log_file_pos = unpacked_data.first;
    artifacts.block_id = unpack_block_id(block_num);

    return artifacts;
  }

  std::string as_hex(uint64_t v)
  {
    std::stringstream ss;
    ss << std::hex << v;
    return ss.str();
  }

  void pack_data(uint64_t block_start_pos, block_attributes_t attributes)
  {
    combined_pos_and_attrs = detail::combine_block_start_pos_with_flags(block_start_pos, attributes);

    FC_ASSERT(block_start_pos == block_log_offset, "Mismatch: block_start_pos: ${block_start_pos} vs block_log_offset: ${block_log_offset}",
              (block_start_pos)(block_log_offset));
    FC_ASSERT(is_compressed == (attributes.flags == block_flags::zstd), "Mismatch: is_compressed: ${is_compressed} vs attributes.flags: ${flags}",
              (is_compressed)("flags", attributes.flags));
    FC_ASSERT(custom_dict_used == (bool)attributes.dictionary_number,
              "Mismatch for: combined_pos_and_attrs: ${combined_pos_and_attrs}: custom_dict_used: ${custom_dict_used} vs attributes.dictionary_number: ${v}",
              (custom_dict_used)("v", (bool)attributes.dictionary_number)("combined_pos_and_attrs", as_hex(combined_pos_and_attrs)));

    if (attributes.dictionary_number)
      FC_ASSERT(dictionary_no == *attributes.dictionary_number, "Mismatch: dictionary_no: ${dictionary_no} vs *attributes.dictionary_number: ${attributes_dictionary_number}",
                (dictionary_no)("attributes_dictionary_number", attributes.dictionary_number));
  }

  block_log_artifacts::block_id_t unpack_block_id(uint32_t block_num) const
  {
    block_log_artifacts::block_id_t block_id;
    
    static_assert(sizeof(block_id._hash) - sizeof(uint32_t) == sizeof(stripped_block_id));

    memcpy(block_id._hash + 1, &stripped_block_id, sizeof(stripped_block_id));

    block_id._hash[0] = fc::endian_reverse_u32(block_num); // store the block num in the ID, 160 bits is plenty for the hash

    return block_id;
  }

  void pack_block_id(uint32_t block_num, block_log_artifacts::block_id_t block_id)
  {
    static_assert(sizeof(block_id._hash) - sizeof(uint32_t) == sizeof(stripped_block_id));
    FC_ASSERT(block_id._hash[0] == fc::endian_reverse_u32(block_num),
      "Malformed id: ${id} for block: ${b}. block_num taken from id: ${n}", ("id", block_id)("b", block_num)
        ("n", fc::endian_reverse_u32(block_id._hash[0])));

    memcpy(&stripped_block_id, block_id._hash + 1, sizeof(stripped_block_id));
  }

};

inline size_t calculate_block_serialized_data_size(const artifact_file_chunk& next_chunk, const artifact_file_chunk& this_chunk)
{
  /// According to block_log format: <serialized_block1><offset_to_prev_block> .. <serialized_blockN><offset_to_prev_blockN>
  return next_chunk.block_log_offset - this_chunk.block_log_offset - sizeof(uint64_t);
}

} /// anonymous


class block_log_artifacts::impl final
{
public:
  void try_to_open(const fc::path& block_log_file_path, bool read_only, const block_log& source_block_provider, uint32_t head_block_num);

  uint32_t read_head_block_num() const
  {
    return _header.head_block_num;
  }

  /** Chunk processor arguments:
  *  - number of first block stating the range
  *  - pointer to buffer holding data chunks read from file
  *  - number of artifact_file_chunk items held in buffer,
  *  - artifact_file_chunk following requested buffer, to allow correctly evaluate last queried block size
  */
  typedef std::function<void(uint32_t, const artifact_file_chunk*, size_t, const artifact_file_chunk&)> artifact_file_chunk_processor_t;

  void process_block_artifacts(uint32_t block_num, uint32_t count, artifact_file_chunk_processor_t processor) const;

  void store_block_artifacts(uint32_t block_num, uint64_t block_log_file_pos, const block_attributes_t& block_attributes, const block_id_t& block_id);

  void update_head_block(uint32_t block_num)
  {
    _header.head_block_num = block_num;
  }

  bool is_open() const
  {
    return _storage_fd != -1;
  }

  void close()
  {
    ilog("Closing a block log artifact file: ${_artifact_file_name} file...", (_artifact_file_name));

    _header.dirty_close = 0;

    flush_header();

    HANDLE_IO((::close(_storage_fd)), "Closing the artifact file");

    _storage_fd = -1;

    ilog("Block log artifact file closed.");
  }

private:
  bool load_header();
  void flush_header() const;

  /// Returns true if generation has been completed, false otherwise
  void generate_file(const block_log& source_block_provider, uint32_t first_block, uint32_t last_block);
  
  void truncate_file(uint32_t last_block);

  void write_data(const std::vector<char>& buffer, off_t offset, const std::string& description) const
  {
    hive::utilities::perform_write(_storage_fd, buffer.data(), buffer.size(), offset, description);
  }

  template <class Data, unsigned int N=1>
  void write_data(const Data& buffer, off_t offset, const std::string& description) const
  {
    hive::utilities::perform_write(_storage_fd, reinterpret_cast<const char*>(&buffer), N*sizeof(Data), offset, description);
  }

  template <class Data>
  void read_data(Data* buffer, off_t offset, const std::string& description) const
  {
    const auto to_read = sizeof(Data);
    auto total_read = hive::utilities::perform_read(_storage_fd, reinterpret_cast<char*>(buffer), to_read, offset, description);

    FC_ASSERT(total_read == to_read, "Incomplete read: expected: ${r}, performed: ${tr}", ("r", to_read)("tr", total_read));
  }

  template <class Data>
  void read_data(Data* item_buffer, size_t item_count, off_t offset, const std::string& description) const
  {
    const auto to_read = item_count*sizeof(Data);
    auto total_read = hive::utilities::perform_read(_storage_fd, reinterpret_cast<char*>(item_buffer), to_read, offset, description);

    FC_ASSERT(total_read == to_read, "Incomplete read: expected: ${r}, performed: ${tr}", ("r", to_read)("tr", total_read));
  }

  size_t calculate_offset(uint32_t block_num) const
  {
    return header_pack_size + artifact_chunk_size*(block_num - 1);
  }

  uint64_t timestamp_ms() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  }

private:
  fc::path _artifact_file_name;
  int _storage_fd = -1; /// file descriptor to the opened file.
  artifact_file_header _header;
  const size_t header_pack_size = sizeof(_header);
  const size_t artifact_chunk_size = sizeof(artifact_file_chunk);
  bool _is_writable = false;
};

void block_log_artifacts::impl::try_to_open(const fc::path& block_log_file_path, bool read_only,
                                            const block_log& source_block_provider, uint32_t head_block_num)
{ try {
  _artifact_file_name = fc::path(block_log_file_path.generic_string() + ".artifacts");
  _is_writable = !read_only;

  int flags = O_RDWR | O_CLOEXEC;
  if (read_only)
    flags = O_RDONLY | O_CLOEXEC;

  _storage_fd = ::open(_artifact_file_name.generic_string().c_str(), flags, 0644);

  if (_storage_fd == -1)
  {
    if (errno == ENOENT)
    {
      wlog("Could not find artifacts file in ${_artifact_file_name}, it will be created and generated from block_log.", (_artifact_file_name));
      _storage_fd = ::open(_artifact_file_name.generic_string().c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
      if (_storage_fd == -1)
        FC_THROW("Error creating block artifacts file ${_artifact_file_name}: ${error}", (_artifact_file_name)("error", strerror(errno)));

      _is_writable = true; //we need to be able to write the header since we're creating a new artifacts file
      _header.dirty_close = 1;
      flush_header();

      /// Generate artifacts file only if some blocks are present in pointed block_log.
      if (head_block_num > 0)
      {
        generate_file(source_block_provider, 1, head_block_num);
        _header.head_block_num = head_block_num;
      }

      flush_header();
    }
    else
    {
      FC_THROW("Error opening block index file ${_artifact_file_name}: ${error}", (_artifact_file_name)("error", strerror(errno)));
    }
  }
  else
  {
    /// The file exists. Lets verify if it can be used immediately or rather shall be regenerated.
    if (load_header())
    {
      _header.dirty_close = 1;
      flush_header();

      if (head_block_num < _header.head_block_num)
      {
        wlog("block_log file is shorter than current block_log.artifact file - the artifact file will be truncated.");

        /// Artifact file is too big. Let's try to truncate it
        truncate_file(head_block_num);

        /// head_block_num must be updated
        _header.head_block_num = head_block_num;
        flush_header();
      }
      else if (head_block_num > _header.head_block_num)
      {
        wlog("block_log file is longer than current block_log.artifact file - artifact file generation will be resumed for range: <${first_block}:${last_block}>.",
             ("first_block", _header.head_block_num + 1)("last_block", head_block_num));
        /// Artifact file is too short - we need to resume its generation
        generate_file(source_block_provider, _header.head_block_num + 1, head_block_num);
        /// head_block_num must be updated
        _header.head_block_num = head_block_num;

        flush_header();
      }
      else
      {
        ilog("block_log and block_log.artifacts files match - no generation needed.");
      }

    }
    else
    {
      wlog("Block artifacts file ${_artifact_file_name} exists, but its header validation failed.", (_artifact_file_name));

      if (read_only)
      {
        FC_THROW("Generation of Block artifacts file ${_artifact_file_name} is disallowed by enforced read-only access.", ("filename", _artifact_file_name));
      }
      else
      {
        ilog("Attempting to overwrite existing artifacts file ${_artifact_file_name} exists...", (_artifact_file_name));

        HANDLE_IO((::close(_storage_fd)), "Closing the artifact file (before truncation)");

        _storage_fd = ::open(_artifact_file_name.generic_string().c_str(), flags | O_TRUNC, 0644);
        if (_storage_fd == -1)
          FC_THROW("Error creating block artifacts file ${_artifact_file_name}: ${error}", (_artifact_file_name)("error", strerror(errno)));

        _header = artifact_file_header();
        _header.dirty_close = 1;
        flush_header();

        generate_file(source_block_provider, 1, head_block_num);
        _header.head_block_num = head_block_num;

        flush_header();
      }
    }
  }
} FC_CAPTURE_AND_RETHROW() }

bool block_log_artifacts::impl::load_header()
{
  try
  {
    read_data(&_header, 0, "Reading the artifact file header");

    ilog("Loaded header containing: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}",
         ("gr", _header.git_version)("major", _header.format_major_version)("minor", _header.format_minor_version)("hb", _header.head_block_num));

    return true;
  }
  catch (const fc::exception& e)
  {
    wlog("Loading the artifact file header failed: ${e}", ("e", e.to_detail_string()));
    return false;
  }
}

void block_log_artifacts::impl::flush_header() const
{ try {
  if (!_is_writable)
    return;

  dlog("Header pack size: ${header_pack_size}", (header_pack_size));

  dlog("Attempting to write header containing: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}, dirty_close: ${d}",
       ("gr", _header.git_version)("major", _header.format_major_version)("minor", _header.format_minor_version)("hb", _header.head_block_num)("d", _header.dirty_close));

  write_data(_header, 0, "Flushing a file header");

  //artifact_file_header _h2(1);
  //ilog("header before read: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}, dirty_close: ${d}",
  //  ("gr", _h2.git_version)("major", _h2.format_major_version)("minor", _h2.format_minor_version)("hb", _h2.head_block_num)("d", _h2.dirty_close));

  //read_data(&_h2, 0, "Reading the artifact file header");

  //ilog("Loaded header containing: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}, dirty_close: ${d}",
  //  ("gr", _h2.git_version)("major", _h2.format_major_version)("minor", _h2.format_minor_version)("hb", _h2.head_block_num)("d", _h2.dirty_close));
} FC_CAPTURE_AND_RETHROW() }

void block_log_artifacts::impl::generate_file(const block_log& source_block_provider, uint32_t first_block, uint32_t last_block)
{
  ilog("Attempting to generate a block artifact file for block range: ${first_block}...${last_block}", (first_block)(last_block));

  FC_ASSERT(first_block <= last_block, "${first_block} <= ${last_block}", (first_block)(last_block));

  uint64_t time_begin = timestamp_ms();
  uint32_t block_count = last_block - first_block + 1;

  uint32_t interrupted_at_block = 0;

  std::mutex queue_mutex;
  std::condition_variable queue_condition;
  struct full_block_with_artifacts
  {
    std::shared_ptr<full_block_type> full_block;
    uint64_t block_log_file_pos;
    block_attributes_t attributes;
  };

#define ARTIFACTS_LOCKFREE // no locks on the reader side of the queue (reader is always the limiting factor)
#ifdef ARTIFACTS_LOCKFREE
  typedef boost::lockfree::queue<full_block_with_artifacts*> queue_type;
  queue_type full_block_queue{10000};
  std::atomic<int> queue_size = { 0 }; // approx full_block_queue size
#else
  std::queue<full_block_with_artifacts*, std::list<full_block_with_artifacts*>> full_block_queue;
#endif
  constexpr int max_blocks_to_prefetch = 10000;

  std::thread writer_thread([&]() {
    fc::set_thread_name("artifact_writer"); // tells the OS the thread's name
    fc::thread::current().set_name("artifact_writer"); // tells fc the thread's name for logging
    std::vector<artifacts_t> plural_of_artifacts;
    std::optional<fc::time_point> last_million_time;
    for (;;)
    {
      full_block_with_artifacts* work_raw_ptr = nullptr;
      {
        std::unique_lock<std::mutex> lock(queue_mutex);
#ifdef ARTIFACTS_LOCKFREE
        while (!appbase::app().is_interrupt_request() && !full_block_queue.pop(work_raw_ptr))
          queue_condition.wait(lock);
        if (appbase::app().is_interrupt_request() || !work_raw_ptr)
        {
          if(work_raw_ptr != nullptr)
            interrupted_at_block = work_raw_ptr->full_block->get_block_num();
          break;
        }
        queue_size.fetch_sub(1, std::memory_order_relaxed);
#else
        while (!appbase::app().is_interrupt_request() && full_block_queue.empty())
          queue_condition.wait(lock);
        work_raw_ptr = full_block_queue.front();
        full_block_queue.pop();
        if (appbase::app().is_interrupt_request())
        {
          if(work_raw_ptr != nullptr)
            interrupted_at_block = work_raw_ptr->full_block->get_block_num();
          break;
        }

        if (!work_raw_ptr)
          break;
#endif
      }
      queue_condition.notify_one();

      std::unique_ptr<full_block_with_artifacts> work(work_raw_ptr);
      store_block_artifacts(work_raw_ptr->full_block->get_block_num(), work_raw_ptr->block_log_file_pos, work_raw_ptr->attributes, work_raw_ptr->full_block->get_block_id());
      if (work_raw_ptr->full_block->get_block_num() % 1000000 == 0)
      {
        if (last_million_time)
        {
          fc::microseconds million_duration = fc::time_point::now() - *last_million_time;
          float blocks_per_usec = 1000000.f / million_duration.count();
          uint32_t seconds_remaining = work_raw_ptr->full_block->get_block_num() / blocks_per_usec / 1000000;
          std::ostringstream blocks_per_sec;
          blocks_per_sec << std::fixed << std::setprecision(2) << (blocks_per_usec * 1000000.f);
          ilog("Processed block ${block_num} at ${blocks_per_sec} blocks/s, estimated ${seconds_remaining}s remaining", 
               ("block_num", work_raw_ptr->full_block->get_block_num())
               ("blocks_per_sec", blocks_per_sec.str())(seconds_remaining));
        }
        else
          ilog("Processed block ${block_num}", ("block_num", work_raw_ptr->full_block->get_block_num()));

        last_million_time = fc::time_point::now();
      }
    }
  });

  source_block_provider.for_each_block([&](uint32_t block_num, const std::shared_ptr<full_block_type>& full_block, uint64_t block_pos, block_attributes_t attributes) {
    if (block_num < first_block)
      return false;

    if (block_num > last_block)
      return true;
#ifdef ARTIFACTS_LOCKFREE
    if (!appbase::app().is_interrupt_request() &&
        queue_size.load(std::memory_order_relaxed) >= max_blocks_to_prefetch)
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      while (!appbase::app().is_interrupt_request() && queue_size.load(std::memory_order_relaxed) >= max_blocks_to_prefetch)
        queue_condition.wait(lock);
    }
    if (appbase::app().is_interrupt_request())
    {
      interrupted_at_block = full_block->get_block_num();
      return false;
    }

    full_block_queue.push(new full_block_with_artifacts{full_block, block_pos, attributes});
    queue_size.fetch_add(1, std::memory_order_relaxed);
#else
    std::unique_lock<std::mutex> lock(queue_mutex);
    while (!appbase::app().is_interrupt_request() && full_block_queue.size() >= max_blocks_to_prefetch)
      queue_condition.wait(lock);
    if (appbase::app().is_interrupt_request())
    {
      interrupted_at_block = full_block->get_block_num();
      return false;
    }

    full_block_queue.push(new full_block_with_artifacts{full_block, block_pos, attributes});
#endif
    blockchain_worker_thread_pool::get_instance().enqueue_work(full_block, blockchain_worker_thread_pool::data_source_type::block_log_for_artifact_generation);
    queue_condition.notify_one();
    return true;
  });
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    full_block_queue.push(nullptr); // signal the writer thread that we've processed the last block
    queue_condition.notify_one();
  }
  
  writer_thread.join();

  uint64_t time_end = timestamp_ms();
  auto elapsed_time = time_end - time_begin;

  if(interrupted_at_block != 0)
    FC_THROW("Block artifact file generation has been INTERRUPTED at block: ${b} and is INCOMPLETE. Execution can't be continued...", ("b", interrupted_at_block));
  else
    ilog("Block artifact file generation finished. ${block_count} blocks processed in time: ${elapsed_time} ms", (block_count)(elapsed_time));
}

void block_log_artifacts::impl::truncate_file(uint32_t last_block)
{
  auto last_chunk_position = calculate_offset(last_block);
  /// File truncate should be done just after last data chunk stored.
  auto truncate_position = last_chunk_position + artifact_chunk_size;

  if (ftruncate(_storage_fd, truncate_position) == -1)
    FC_THROW("Error truncating block artifact file: ${error}", ("error", strerror(errno)));
}

void block_log_artifacts::impl::process_block_artifacts(uint32_t block_num, uint32_t count, artifact_file_chunk_processor_t processor) const
{
  auto chunk_position = calculate_offset(block_num);

  std::vector<artifact_file_chunk> chunk_buffer;

  chunk_buffer.resize(count + 1);

  read_data(chunk_buffer.data(), chunk_buffer.size(), chunk_position, "Reading an artifact data chunk");

  const artifact_file_chunk& next_chunk = chunk_buffer.back();

  processor(block_num, chunk_buffer.data(), count, next_chunk);
}

void block_log_artifacts::impl::store_block_artifacts(uint32_t block_num, uint64_t block_log_file_pos,
                                                      const block_attributes_t& block_attrs, const block_id_t& block_id)
{
  artifact_file_chunk data_chunk;

  data_chunk.pack_data(block_log_file_pos, block_attrs);
  data_chunk.pack_block_id(block_num, block_id);

  auto write_position = calculate_offset(block_num);

  write_data(data_chunk, write_position, "Wrting the artifact file datachunk");
}


block_log_artifacts::block_log_artifacts() : _impl(std::make_unique<impl>())
{
}

block_log_artifacts::~block_log_artifacts()
{
  if (_impl->is_open())
    _impl->close();
}

block_log_artifacts::block_log_artifacts_ptr_t block_log_artifacts::open(const fc::path& block_log_file_path,
  bool read_only, const block_log& source_block_provider, uint32_t head_block_num)
{
  block_log_artifacts_ptr_t block_artifacts(new block_log_artifacts);

  block_artifacts->_impl->try_to_open(block_log_file_path, read_only, source_block_provider, head_block_num);

  return block_artifacts;
}

/// Allows to read a number of last block the artifacts are stored for.
uint32_t block_log_artifacts::read_head_block_num() const
{
  return _impl->read_head_block_num();
}

block_log_artifacts::artifacts_t block_log_artifacts::read_block_artifacts(uint32_t block_num) const
{
  artifacts_t artifacts;

  _impl->process_block_artifacts(block_num, 1, 
    [&artifacts](uint32_t block_num, const artifact_file_chunk* chunk_buffer, size_t chunk_count, const artifact_file_chunk& next_chunk) -> void
  {
    artifacts = chunk_buffer->unpack_data(block_num);
    artifacts.block_serialized_data_size = calculate_block_serialized_data_size(next_chunk, *chunk_buffer);
  });

  FC_ASSERT(artifacts.block_id != block_id_type(), "Broken block id - probably artifact file is damaged");

  return artifacts;
}

block_log_artifacts::artifact_container_t
block_log_artifacts::read_block_artifacts(uint32_t start_block_num, uint32_t block_count, size_t* block_size_sum /*= nullptr*/) const
{
  artifact_container_t storage;
  storage.reserve(block_count);

  size_t size_sum = 0;

  _impl->process_block_artifacts(start_block_num, block_count,
    [&storage, &size_sum](uint32_t block_num, const artifact_file_chunk* chunk_buffer, size_t chunk_count, const artifact_file_chunk& next_chunk) -> void
  {
    /// Process first chunk before loop, since inside we have to backreference prev. item
    storage.emplace_back(chunk_buffer->unpack_data(block_num));

    size_t prev_block_size = 0;

    for(size_t i = 1; i < chunk_count; ++i)
    {
      const artifact_file_chunk& chunk = chunk_buffer[i];

      prev_block_size = calculate_block_serialized_data_size(chunk, chunk_buffer[i-1]);

      size_sum += prev_block_size + sizeof(uint64_t);

      storage.back().block_serialized_data_size = prev_block_size;

      storage.emplace_back(chunk.unpack_data(block_num + i));
    }

    /// supplement last produced artifacts by block size
    prev_block_size = calculate_block_serialized_data_size(next_chunk, chunk_buffer[chunk_count - 1]);
    storage.back().block_serialized_data_size = prev_block_size;

    size_sum += prev_block_size + sizeof(uint64_t);
  });

  if(block_size_sum != nullptr)
    *block_size_sum = size_sum;

  return storage;
}

void block_log_artifacts::store_block_artifacts(uint32_t block_num, uint64_t block_log_file_pos, const block_attributes_t& block_attributes,
  const block_id_t& block_id)
{
  _impl->store_block_artifacts(block_num, block_log_file_pos, block_attributes, block_id);
  _impl->update_head_block(block_num);
}

}}


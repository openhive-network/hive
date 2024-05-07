#include <hive/chain/block_log_artifacts.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/full_block.hpp>

#include <hive/utilities/git_revision.hpp>
#include <hive/utilities/io_primitives.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <appbase/application.hpp>

#include <fc/bitutil.hpp>
#include <fc/thread/thread.hpp>
#include <fc/io/json.hpp>

#include <boost/lockfree/spsc_queue.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

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

constexpr uint16_t FORMAT_MAJOR = 1;
constexpr uint16_t FORMAT_MINOR = 1;

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
    head_block_num = 0; /// number of newest (head) block the file holds information for
    dirty_close = 0;
    tail_block_num = 0;
    generating_interrupted_at_block = 0;
  }

  uint32_t head_block_num; /// number of newest (head) block the file holds information for
  char git_version[41]; /// version of a tool which constructed given file
  uint8_t format_major_version; /// version info of storage format (to allow potential upgrades in the future)
  uint8_t format_minor_version;
  uint8_t dirty_close;
  uint32_t tail_block_num;  // start of artifacts block range. If artifacts file is under generating process, it points to lower end of generation range (1 or previous head_block),
  uint32_t generating_interrupted_at_block; // if file is under generating process, it stores data of last processed block from block_log, in case if generating is interrupted,
                                            // it will be continued from block number pointed by that variable. If generating process is finished, it should be set to 0.
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

  impl( appbase::application& app );

  void open(const fc::path& block_log_file_path, const block_log& source_block_provider, const bool read_only, const bool full_match_verification, hive::chain::blockchain_worker_thread_pool& thread_pool);

  fc::path get_artifacts_file() const
  {
    return _artifact_file_name;
  }

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

  block_log_artifacts::artifacts_t read_block_artifacts(uint32_t block_num) const;

  void update_head_block(uint32_t block_num)
  {
    FC_ASSERT(_is_writable, "Block log artifacts was opened in read only mode.");
    _header.head_block_num = block_num;
  }

  bool is_open() const
  {
    return _storage_fd != -1;
  }

  void close()
  {
    ilog("Closing a block log artifact file: ${_artifact_file_name} file...", (_artifact_file_name));
    if (_is_writable)
    {
      _header.dirty_close = 0;
      flush_header();
    }
    HANDLE_IO((::close(_storage_fd)), "Closing the artifact file");

    _storage_fd = -1;

    ilog("Block log artifact file closed.");
  }

  void truncate_file(uint32_t last_block);

  const artifact_file_header& get_artifacts_header() const
  {
    return _header;
  }

  void flush_header() const;

  void set_block_num_to_file_pos_offset(uint32_t block_num_to_file_pos_offset)
  {
    ilog("Setting _block_num_to_file_pos_offset to ${block_num_to_file_pos_offset}", (block_num_to_file_pos_offset));
    _block_num_to_file_pos_offset = block_num_to_file_pos_offset;
  }

private:
  bool load_header();

  void generate_artifacts_file(const block_log& source_block_provider, hive::chain::blockchain_worker_thread_pool& thread_pool);
  void verify_if_blocks_from_block_log_matches_artifacts(const block_log& source_block_provider, const bool full_match_verification, const bool use_block_log_head_num) const;
  
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
    return header_pack_size + artifact_chunk_size*(block_num - 1 - _block_num_to_file_pos_offset);
  }

  uint64_t timestamp_ms() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  }

  uint32_t calculate_tail_block_num(uint32_t new_tail) const
  {
    return _block_num_to_file_pos_offset + new_tail;
  }

private:
  fc::path _artifact_file_name;
  int _storage_fd = -1; /// file descriptor to the opened file.
  artifact_file_header _header;
  const size_t header_pack_size = sizeof(_header);
  const size_t artifact_chunk_size = sizeof(artifact_file_chunk);
  boost::interprocess::file_lock _flock;
  uint32_t _block_num_to_file_pos_offset = 0; /// Zero when given block log file starts with block #1.
  bool _is_writable = false;

  appbase::application& theApp;
};

block_log_artifacts::impl::impl( appbase::application& app ): theApp( app )
{

}

void block_log_artifacts::impl::open(const fc::path& block_log_file_path, const block_log& source_block_provider, const bool read_only, const bool full_match_verification, hive::chain::blockchain_worker_thread_pool& thread_pool)
{
  try {
  set_block_num_to_file_pos_offset(
    block_log_file_name_info::get_first_block_num_for_file_name(block_log_file_path)-1
  );
  _artifact_file_name = fc::path(block_log_file_path.generic_string() + block_log_file_name_info::_artifacts_extension.c_str());
  FC_ASSERT(!fc::is_directory(_artifact_file_name), "${_artifact_file_name} should point to block_log.artifacts file, not directory", (_artifact_file_name));
  _is_writable = !read_only;

  const auto head_block = source_block_provider.head();
  const uint32_t block_log_head_block_num = head_block ? head_block->get_block_num() : 0;

  if (read_only)
  {
    ilog("Opening artifacts file ${_artifact_file_name} in read only mode ...", (_artifact_file_name));
    _storage_fd = ::open(_artifact_file_name.generic_string().c_str(), O_RDONLY | O_CLOEXEC, 0644);

    if (_storage_fd == -1)
      FC_THROW("Cannot open artifacts file in read only mode. File path: ${_artifact_file_name}, error: ${error}", (_artifact_file_name)("error", strerror(errno)));

    _flock = boost::interprocess::file_lock(_artifact_file_name.generic_string().c_str());
    if (!_flock.try_lock_sharable())
      FC_THROW("Unable to get sharable access to artifacts file: ${file_cstr} (some other process opened artifacts in RW mode probably)", ("file_cstr", _artifact_file_name.generic_string().c_str()));

    if (!load_header())
      FC_THROW("Cannot load header of artifacts file: ${_artifact_file_name}", (_artifact_file_name));

    if (block_log_head_block_num != _header.head_block_num)
      FC_THROW("Artifacts file has other head block num: ${artifacts_head_block} than block_log: ${block_log_head_block_num}", ("artifacts_head_block", _header.head_block_num)(block_log_head_block_num));

    if (_header.generating_interrupted_at_block)
      FC_THROW("Artifacts file generating process is not finished.");
    
    verify_if_blocks_from_block_log_matches_artifacts(source_block_provider, full_match_verification, false);
  }

  else
  {
    ilog("Opening artifacts file ${_artifact_file_name} in read & write mode ...", (_artifact_file_name));
    _storage_fd = ::open(_artifact_file_name.generic_string().c_str(), O_RDWR | O_CLOEXEC, 0644);

    if (_storage_fd == -1)
    {
      if (errno == ENOENT)
      {
        wlog("Could not find artifacts file in ${_artifact_file_name}. Creating new artifacts file ...", (_artifact_file_name));
        _storage_fd = ::open(_artifact_file_name.generic_string().c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);

        if (_storage_fd == -1)
          FC_THROW("Error creating block artifacts file ${_artifact_file_name}: ${error}", (_artifact_file_name)("error", strerror(errno)));

        _flock = boost::interprocess::file_lock(_artifact_file_name.generic_string().c_str());
        if (!_flock.try_lock())
          FC_THROW("Unable to get read & write access to artifacts file: ${file_cstr} (some other process opened artifacts probably)", ("file_cstr", _artifact_file_name.generic_string().c_str()));

        _header.dirty_close = 1;
        flush_header();

        /// Generate artifacts file only if some blocks are present in pointed block_log.
        if (block_log_head_block_num > 0)
        {
          _header.tail_block_num = calculate_tail_block_num(1);
          _header.head_block_num = block_log_head_block_num;
          flush_header();
          generate_artifacts_file(source_block_provider, thread_pool);
        }
      }
      else
        FC_THROW("Error opening block artifacts file ${_artifact_file_name}: ${error}", (_artifact_file_name)("error", strerror(errno)));
    }
    else
    {
      /// The file exists. Lets verify if it can be used immediately or rather shall be regenerated.

      _flock = boost::interprocess::file_lock(_artifact_file_name.generic_string().c_str());
      if (!_flock.try_lock())
        FC_THROW("Unable to get read & write access to artifacts file: ${file_cstr} (some other process opened artifacts probably)", ("file_cstr", _artifact_file_name.generic_string().c_str()));

      if (load_header())
      {
        _header.dirty_close = 1;
        flush_header();

        if (block_log_head_block_num < _header.head_block_num)
        {
          wlog("block_log head block num: ${block_log_head_block_num}, block_log.artifact head block num: ${artifacts_head_block_num}. Block log file is shorter, the artifact file will be truncated.",
              (block_log_head_block_num)("artifacts_head_block_num", _header.head_block_num));
          
          if (_header.generating_interrupted_at_block > block_log_head_block_num)
            FC_THROW("Artifacts file has been filled up to ${interrupted_at_block} block, truncating artifacts file will result an empty file. Remove artifacts file and create artifacts from the beggining.", ("interrupted_at_block", _header.generating_interrupted_at_block));

          verify_if_blocks_from_block_log_matches_artifacts(source_block_provider, full_match_verification, true);
          truncate_file(block_log_head_block_num);

          if (_header.generating_interrupted_at_block)
            generate_artifacts_file(source_block_provider, thread_pool);
        }
        else
        {
          verify_if_blocks_from_block_log_matches_artifacts(source_block_provider, full_match_verification, false);

          if (_header.generating_interrupted_at_block)
            generate_artifacts_file(source_block_provider, thread_pool);

          if (block_log_head_block_num > _header.head_block_num && !_header.generating_interrupted_at_block && !theApp.is_interrupt_request())
          {
            wlog("block_log file is longer than current block_log.artifact file. Artifacts head block num: ${header_head_block_num}, block log head block num: ${block_log_head_block_num}.",
                ("header_head_block_num", _header.head_block_num)(block_log_head_block_num));

            _header.tail_block_num = _header.head_block_num ? _header.head_block_num : calculate_tail_block_num(1);
            _header.head_block_num = block_log_head_block_num;
            flush_header();
            generate_artifacts_file(source_block_provider, thread_pool);
          }
        }
      }
      else
      {
        wlog("Block artifacts file ${_artifact_file_name} exists, but its header validation failed.", (_artifact_file_name));
        ilog("Attempting to overwrite existing artifacts file ${_artifact_file_name} ...", (_artifact_file_name));
        HANDLE_IO((::close(_storage_fd)), "Closing the artifact file (before truncation)");
        _storage_fd = ::open(_artifact_file_name.generic_string().c_str(), O_RDWR | O_CLOEXEC | O_TRUNC, 0644);

        if (_storage_fd == -1)
          FC_THROW("Error creating block artifacts file ${_artifact_file_name}: ${error}", (_artifact_file_name)("error", strerror(errno)));
        else
        {
          _header = artifact_file_header();
          _header.dirty_close = 1;
          flush_header();

          if (block_log_head_block_num)
          {
            _header.tail_block_num = calculate_tail_block_num(1);
            _header.head_block_num = block_log_head_block_num;
            flush_header();
            generate_artifacts_file(source_block_provider, thread_pool);
          }
        }
      }
    }
  }
} FC_CAPTURE_AND_RETHROW() }

bool block_log_artifacts::impl::load_header()
{
  try
  {
    read_data(&_header, 0, "Reading the artifact file header");

    if (_header.format_major_version != FORMAT_MAJOR || _header.format_minor_version != FORMAT_MINOR)
      FC_THROW("Artifacts file header version (${major}.${minor}) must match to expected version(${emajor}.${eminor}) by hived.",
              ("major", _header.format_major_version)("minor", _header.format_minor_version)("emajor", FORMAT_MAJOR)("eminor", FORMAT_MINOR));

    ilog("Loaded header containing: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}, tail_block_num: ${tbn}, generating_interrupted_at_block: ${giat}, dirty_closed: ${dc}",
         ("gr", _header.git_version)("major", _header.format_major_version)("minor", _header.format_minor_version)("hb", _header.head_block_num)("tbn", _header.tail_block_num)
         ("giat", _header.generating_interrupted_at_block)("dc", _header.dirty_close));

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
  FC_ASSERT(_is_writable);
  dlog("Attempting to write header (pack_size: ${header_pack_size}) containing: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}, tail_block_num: ${tb}, generating_interrupted_at_block: ${giat}, dirty_close: ${d}",
      (header_pack_size)("gr", _header.git_version)("major", _header.format_major_version)("minor", _header.format_minor_version)("hb", _header.head_block_num)("tb", _header.tail_block_num)("giat", _header.generating_interrupted_at_block)("d", _header.dirty_close));
  write_data(_header, 0, "Flushing a file header");

  //artifact_file_header _h2(1);
  //ilog("header before read: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}, dirty_close: ${d}",
  //  ("gr", _h2.git_version)("major", _h2.format_major_version)("minor", _h2.format_minor_version)("hb", _h2.head_block_num)("d", _h2.dirty_close));

  //read_data(&_h2, 0, "Reading the artifact file header");

  //ilog("Loaded header containing: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}, dirty_close: ${d}",
  //  ("gr", _h2.git_version)("major", _h2.format_major_version)("minor", _h2.format_minor_version)("hb", _h2.head_block_num)("d", _h2.dirty_close));
} FC_CAPTURE_AND_RETHROW() }

void block_log_artifacts::impl::generate_artifacts_file(const block_log& source_block_provider, hive::chain::blockchain_worker_thread_pool& thread_pool)
{
  const uint32_t starting_block_num = _header.generating_interrupted_at_block ? _header.generating_interrupted_at_block : _header.head_block_num;
  const uint32_t target_block_num = _header.tail_block_num;
  const fc::optional<uint64_t> starting_block_position = _header.generating_interrupted_at_block ? read_block_artifacts(_header.generating_interrupted_at_block).block_log_file_pos : fc::optional<uint64_t>();
  ilog("Generating block log artifacts file from block ${starting_block_num} to ${target_block_num}", (starting_block_num)(target_block_num));

  uint32_t processed_blocks_count = 0;
  const uint64_t time_begin = timestamp_ms();
  bool generating_interrupted = false;

  std::mutex queue_mutex;
  std::condition_variable queue_condition;
  struct full_block_with_artifacts
  {
    std::shared_ptr<full_block_type> full_block;
    uint64_t block_log_file_pos;
    block_attributes_t attributes;
  };

  typedef boost::lockfree::spsc_queue<std::shared_ptr<full_block_with_artifacts>> queue_type;
  constexpr size_t MAX_BLOCK_TO_PREFETCH = 10000;
  queue_type full_block_queue{MAX_BLOCK_TO_PREFETCH};
  std::atomic<size_t> queue_size = { 0 }; // approx full_block_queue size

  std::thread artifacts_writer_thread([&]() {
    fc::set_thread_name("artifact_writer"); // tells the OS the thread's name
    fc::thread::current().set_name("artifact_writer"); // tells fc the thread's name for logging

    constexpr uint32_t BLOCKS_COUNT_INTERVAL_FOR_ARTIFACTS_SAVE = 1000000;
    size_t idx = 0;
    uint32_t current_block_number = starting_block_num;

    while(current_block_number > target_block_num)
    {
      std::shared_ptr<full_block_with_artifacts> block_with_artifacts;
      {
        std::unique_lock<std::mutex> lock(queue_mutex);
        while (!theApp.is_interrupt_request() && !full_block_queue.pop(block_with_artifacts))
          queue_condition.wait(lock);

        if (theApp.is_interrupt_request() || !block_with_artifacts)
        {
          ilog("Artifacts file generation interrupted at block: ${current_block_number}", (current_block_number));
          generating_interrupted = true;
          break;
        }

        queue_size.fetch_sub(1, std::memory_order_relaxed);
      }
      queue_condition.notify_one();
      current_block_number = block_with_artifacts->full_block->get_block_num();
      store_block_artifacts(current_block_number, block_with_artifacts->block_log_file_pos, block_with_artifacts->attributes, block_with_artifacts->full_block->get_block_id());

      if (idx >= BLOCKS_COUNT_INTERVAL_FOR_ARTIFACTS_SAVE)
      {
        ilog("Artifact generation just processed block ${current_block_number}. Processed blocks count: ${processed_blocks_count}, Target block: ${target_block_num}", (current_block_number)(processed_blocks_count)(target_block_num));
        idx = 0;
        _header.generating_interrupted_at_block = current_block_number;
        flush_header();
      }

      ++processed_blocks_count;
      ++idx;
    }

    _header.generating_interrupted_at_block = current_block_number;
  });

  auto block_processor = [&](const std::shared_ptr<full_block_type>& full_block, const uint64_t block_pos, const uint32_t block_num, const block_attributes_t attributes) -> bool
  {
    if (!theApp.is_interrupt_request() && queue_size.load(std::memory_order_relaxed) >= MAX_BLOCK_TO_PREFETCH)
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      while (!theApp.is_interrupt_request() && queue_size.load(std::memory_order_relaxed) >= MAX_BLOCK_TO_PREFETCH)
        queue_condition.wait(lock);
    }
    if (theApp.is_interrupt_request())
      return false;

    std::shared_ptr<full_block_with_artifacts> block_with_artifacts(new full_block_with_artifacts{full_block, block_pos, attributes});
    full_block_queue.push(block_with_artifacts);
    queue_size.fetch_add(1, std::memory_order_relaxed);
    thread_pool.enqueue_work(full_block, blockchain_worker_thread_pool::data_source_type::block_log_for_artifact_generation);
    queue_condition.notify_one();
    return true;
  };

  source_block_provider.read_blocks_data_for_artifacts_generation(block_processor, target_block_num, starting_block_num, starting_block_position);
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    full_block_queue.push(nullptr); // backup signal that all blocks were processed.
    queue_condition.notify_one();
  }
  
  artifacts_writer_thread.join();

  const uint64_t time_end = timestamp_ms();
  const auto elapsed_time = time_end - time_begin;

  if (!generating_interrupted)
  {
    _header.generating_interrupted_at_block = 0;
    _header.tail_block_num = calculate_tail_block_num(1);
    flush_header();
  }

  ilog("Block artifact file generation finished. Elapsed time: ${elapsed_time} ms. Processed blocks count: ${processed_blocks_count}. Generation interrupted: ${was_interrupted}.",
    (elapsed_time)(processed_blocks_count)("was_interrupted", (static_cast<bool>(_header.generating_interrupted_at_block))));
}

void block_log_artifacts::impl::verify_if_blocks_from_block_log_matches_artifacts(const block_log& source_block_provider, const bool full_match_verification, const bool use_block_log_head_num) const
{
  constexpr uint32_t BLOCKS_SAMPLE_AMOUNT = 10;

  if (!full_match_verification && _header.head_block_num < BLOCKS_SAMPLE_AMOUNT + 1)
    return;

  uint32_t first_block_to_verify, last_block_num_to_verify;

  if (full_match_verification)
  {
    first_block_to_verify = _header.head_block_num - 1;
    last_block_num_to_verify = calculate_tail_block_num(1);
  }
  else
  {
    first_block_to_verify = use_block_log_head_num ? source_block_provider.head()->get_block_num() - 1 : _header.head_block_num - 1;
    last_block_num_to_verify = std::max<uint32_t>(first_block_to_verify - BLOCKS_SAMPLE_AMOUNT, calculate_tail_block_num(1));
  }

  FC_ASSERT(last_block_num_to_verify > _header.generating_interrupted_at_block, "block_log.artifacts and block_log files do not match, since artifacts are generated for shorter block range than present in block_log file.");

  ilog("Starting deep verification of already collected artifacts for the block range: ${first_block_to_verify} : ${last_block_num_to_verify}. Any error during this process means that the artifacts don't match the block_log.",
      (first_block_to_verify)(last_block_num_to_verify));

  uint32_t block_num = first_block_to_verify;

  try
  {
    uint32_t counter = 0;

    while(block_num > last_block_num_to_verify)
    {
      const auto block_artifacts = read_block_artifacts(block_num);
      const auto full_block = source_block_provider.read_block_by_offset(block_artifacts.block_log_file_pos, block_artifacts.block_serialized_data_size, block_artifacts.attributes);
      if (full_block->get_block_id() != block_artifacts.block_id)
        FC_THROW("Full block got by offset has malformed ID");
      if (full_block->has_compressed_block_data())
      {
        const auto& compressed_block_data = full_block->get_compressed_block();
        if (compressed_block_data.compressed_size != block_artifacts.block_serialized_data_size)
          FC_THROW("Full block got by offset has malformed compress block size!");
        if (compressed_block_data.compression_attributes.flags != block_artifacts.attributes.flags)
          FC_THROW("Full block got by offset has malformed compress block attributes flags!");
        if (compressed_block_data.compression_attributes.dictionary_number != block_artifacts.attributes.dictionary_number)
          FC_THROW("Full block got by offset has malformed compress block attributes dictionary number!");
      }
      else if (full_block->get_uncompressed_block_size() != block_artifacts.block_serialized_data_size)
        FC_THROW("Full block got by offset has malformed uncompressed block size!");

      --block_num;
      ++counter;

      if (counter >= 1000000)
      {
        dlog("artifacts verification in progress - just verified artifacts for block number: ${block_num}", (block_num));
        counter = 0;
      }
    }
  }
  catch (const fc::exception& e)
  {
    FC_THROW("Block_log doesn't match current artifacts file. Cannot get block: ${block_num} using already stored artifacts. FC error: ${what}", (block_num)("what", e.what()));
  }
  catch (...)
  {
    const auto current_exception = std::current_exception();

    if (current_exception)
    {
      try
      {
        std::rethrow_exception(current_exception);
      }
      catch( const std::exception& e )
      {
        FC_THROW("Block_log doesn't match current artifacts file. Cannot get block: ${block_num} using already stored artifacts. Error: ${what}", (block_num)("what", e.what()));
      }
    }
    else
      FC_THROW("Block_log doesn't match current artifacts file. Cannot get block: ${block_num} using already stored artifacts. Cannot get details about error.", (block_num));
  }

  ilog("Artifacts file matches block_log file.");
}

void block_log_artifacts::impl::truncate_file(uint32_t last_block)
{
  FC_ASSERT(_is_writable, "Block log artifacts was opened in read only mode.");
  auto last_chunk_position = calculate_offset(last_block);
  /// File truncate should be done just after last data chunk stored.
  auto truncate_position = last_chunk_position + artifact_chunk_size;

  if (ftruncate(_storage_fd, truncate_position) == -1)
    FC_THROW("Error truncating block artifact file: ${error}", ("error", strerror(errno)));

  /// head_block_num must be updated
  update_head_block(last_block);
  if (_header.generating_interrupted_at_block > _header.head_block_num)
    _header.generating_interrupted_at_block = _header.head_block_num;

  flush_header();
}

void block_log_artifacts::impl::process_block_artifacts(uint32_t block_num, uint32_t count, artifact_file_chunk_processor_t processor) const
{
  FC_ASSERT(block_num != _header.head_block_num, "It's not possible to read head block artifacts.");

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
  FC_ASSERT(_is_writable, "Block log artifacts was opened in read only mode.");

  if (_header.generating_interrupted_at_block && (block_num > _header.head_block_num))
    FC_THROW("Cannot store new artifacts if generating process isn't finished.");
  // Update tail_block_num to calculate_tail_block_num(1) when artifacts was not generated and from beggining we store artifacts.
  if (!_header.tail_block_num)
    _header.tail_block_num = calculate_tail_block_num(1);

  artifact_file_chunk data_chunk;
  data_chunk.pack_data(block_log_file_pos, block_attrs);
  data_chunk.pack_block_id(block_num, block_id);

  auto write_position = calculate_offset(block_num);
  write_data(data_chunk, write_position, "Wrting the artifact file datachunk");
}

block_log_artifacts::artifacts_t block_log_artifacts::impl::read_block_artifacts(uint32_t block_num) const
{
  artifacts_t artifacts;

  process_block_artifacts(block_num, 1,
    [&artifacts](uint32_t block_num, const artifact_file_chunk* chunk_buffer, size_t chunk_count, const artifact_file_chunk& next_chunk) -> void
  {
    artifacts = chunk_buffer->unpack_data(block_num);
    artifacts.block_serialized_data_size = calculate_block_serialized_data_size(next_chunk, *chunk_buffer);
  });

  FC_ASSERT(artifacts.block_id != block_id_type(), "Broken block id - probably artifact file is damaged");

  return artifacts;
}


block_log_artifacts::block_log_artifacts( appbase::application& app ) : _impl(std::make_unique<impl>( app ))
{
}

block_log_artifacts::~block_log_artifacts()
{
  if (_impl->is_open())
    _impl->close();
}

block_log_artifacts::block_log_artifacts_ptr_t block_log_artifacts::open(const fc::path& block_log_file_path, const block_log& source_block_provider, const bool read_only, const bool full_match_verification, appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool)
{
  block_log_artifacts_ptr_t block_artifacts(new block_log_artifacts( app ));
  block_artifacts->_impl->open(block_log_file_path, source_block_provider, read_only, full_match_verification, thread_pool );
  return block_artifacts;
}

fc::path block_log_artifacts::get_artifacts_file() const
{
  return _impl->get_artifacts_file();
}

/// Allows to read a number of last block the artifacts are stored for.
uint32_t block_log_artifacts::read_head_block_num() const
{
  return _impl->read_head_block_num();
}

block_log_artifacts::artifacts_t block_log_artifacts::read_block_artifacts(uint32_t block_num) const
{
  return _impl->read_block_artifacts(block_num);
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
                                                const block_id_t& block_id, const bool is_at_live_sync)
{
  _impl->store_block_artifacts(block_num, block_log_file_pos, block_attributes, block_id);
  _impl->update_head_block(block_num);

  if (is_at_live_sync)
    _impl->flush_header();
  else
  {
    constexpr uint32_t BLOCKS_COUNT_INTERVAL_FOR_FLUSH = 100000;
    static uint32_t stored_blocks_counter = 0;
    ++stored_blocks_counter;

    if (stored_blocks_counter > BLOCKS_COUNT_INTERVAL_FOR_FLUSH)
    {
      _impl->flush_header();
      stored_blocks_counter = 0;
    }
  }
}

void block_log_artifacts::truncate(uint32_t new_head_block_num)
{
  dlog("truncating block log to ${new_head_block_num} blocks", (new_head_block_num));
  _impl->truncate_file(new_head_block_num);
}

std::string block_log_artifacts::get_artifacts_contents(const fc::optional<uint32_t>& starting_block_number, const fc::optional<uint32_t>& ending_block_number, bool header_only) const
{
  try
  {
    fc::mutable_variant_object result;

    const auto header = _impl->get_artifacts_header();
    const uint32_t artifacts_head_block_num = header.head_block_num;
    {
      fc::mutable_variant_object header_data;
      header_data["head_block_num"] = artifacts_head_block_num;
      header_data["git_version"] = header.git_version;
      header_data["format_major_version"] = header.format_major_version;
      header_data["format_minor_version"] = header.format_minor_version;
      header_data["dirty_close"] = header.dirty_close;
      header_data["tail_block_num"] = header.tail_block_num;
      header_data["generating_interrupted_at_block"] = header.generating_interrupted_at_block;
      result["header"] = header_data;
    }

    if (!header_only)
    {
      const uint32_t artifacts_head_block_number = read_head_block_num();
      const uint32_t start_block_num = starting_block_number ? *starting_block_number : 1;
      const uint32_t end_block_num = ending_block_number ? *ending_block_number : (artifacts_head_block_number - 1);

      FC_ASSERT(start_block_num <= end_block_num, "${start_block_num} <= ${end_block_num}", (start_block_num)(end_block_num));
      FC_ASSERT(end_block_num <= artifacts_head_block_number, "${end_block_num} <= ${artifacts_head_block_number}", (end_block_num)(artifacts_head_block_number));

      std::vector<fc::variant> artifacts_data;
      artifacts_data.reserve(artifacts_head_block_number);

      for (uint32_t block_num = start_block_num; block_num <= end_block_num; ++block_num)
      {
        const auto artifacts = read_block_artifacts(block_num);
        fc::mutable_variant_object obj;
        obj["block_num"] = block_num;
        obj["block_id"] = artifacts.block_id.str();
        obj["attributes"] = fc::json::to_pretty_string(artifacts.attributes);
        obj["pos"] = artifacts.block_log_file_pos;
        obj["size"] = artifacts.block_serialized_data_size;
        artifacts_data.push_back(obj);
      }

      result["artifacts"] = artifacts_data;
    }
    return fc::json::to_pretty_string(result);
  }
  FC_CAPTURE_AND_RETHROW()
}

}}


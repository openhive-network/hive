#include <hive/chain/block_log_artifacts.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/full_block.hpp>

#include <hive/utilities/git_revision.hpp>
#include <hive/utilities/io_primitives.hpp>

#include <fc/bitutil.hpp>

#include <fcntl.h>

#include <chrono>
#include <future>
#include <map>
#include <thread>
#include <vector>

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
    FC_ASSERT(custom_dict_used == unpacked_data.second.dictionary_number.valid());
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
      ("block_start_pos", block_start_pos)("block_log_offset", block_log_offset));
    FC_ASSERT(is_compressed == (attributes.flags == block_flags::zstd), "Mismatch: is_compressed: ${is_compressed} vs attributes.flags: ${f}",
      ("is_compressed", is_compressed)("f", attributes.flags));
    FC_ASSERT(custom_dict_used == attributes.dictionary_number.valid(), "Mismatch for: combined_pos_and_attrs: ${combined_pos_and_attrs}: custom_dict_used: ${c} vs attributes.dictionary_number.valid: ${v}",
      ("c", custom_dict_used)("v", attributes.dictionary_number.valid())("combined_pos_and_attrs", as_hex(combined_pos_and_attrs)));

    if(attributes.dictionary_number.valid())
    {
      FC_ASSERT(dictionary_no == *attributes.dictionary_number,
        "Mismatch: dictionary_no: ${dictionary_no} vs *attributes.dictionary_number: ${ad}", ("dictionary_no", dictionary_no)("ad", *attributes.dictionary_number));
    }
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

  bool is_open() const
  {
    return _storage_fd != -1;
  }

  static void on_destroy(block_log_artifacts* bla)
  {
    FC_ASSERT(bla);
    FC_ASSERT(bla->_impl);
    
    if(bla->_impl->is_open())
      bla->_impl->close();

    delete bla;
  }

  void close()
  {
    ilog("Closing a block log artifact file: ${f} file...", ("f", _artifact_file_name.generic_string()));

    _header.dirty_close = 0;

    flush_header();

    HANDLE_IO((::close(_storage_fd)), "Closing the artifact file");

    _storage_fd = -1;

    ilog("Block log artifact file closed.");
  }

private:
  bool load_header();
  void flush_header() const;

  void generate_file(const block_log& source_block_provider, uint32_t first_block, uint32_t last_block);
  
  void flush_data_chunks(uint32_t min_block_num, const std::vector<artifacts_t>& data);

  typedef std::pair<uint32_t, std::vector<artifacts_t>> worker_thread_result;

  static void woker_thread_body(const block_log& block_provider, uint32_t min_block_num, std::vector<artifacts_t> data,
    std::promise<worker_thread_result>);

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
{
  _artifact_file_name = fc::path(block_log_file_path.generic_string() + ".artifacts");
  _is_writable = read_only == false;

  int flags = O_RDWR | O_CLOEXEC;
  if(read_only)
    flags = O_RDONLY | O_CLOEXEC;

  _storage_fd = ::open(_artifact_file_name.generic_string().c_str(), flags, 0644);

  if(_storage_fd == -1)
  {
    if(errno == ENOENT)
    {
      if(read_only)
      {
        FC_THROW("Block artifacts file ${filename} is missing, but its creation is disallowed by enforced read-only access.",
          ("filename", _artifact_file_name));
      }
      else
      {
        wlog("Could not find artifacts file in ${path}, it will be created and generated from block_log.", ("path", _artifact_file_name));
        _storage_fd = ::open(_artifact_file_name.generic_string().c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        if(_storage_fd == -1)
          FC_THROW("Error creating block artifacts file ${filename}: ${error}", ("filename", _artifact_file_name)("error", strerror(errno)));

        _header.dirty_close = 1;
        flush_header();

        /// Generate artifacts file only if some blocks are present in pointed block_log.
        if(head_block_num > 0)
          generate_file(source_block_provider, 1, head_block_num);

        _header.head_block_num = head_block_num;
        flush_header();
      }
    }
    else
    {
      FC_THROW("Error opening block index file ${filename}: ${error}", ("filename", _artifact_file_name)("error", strerror(errno)));
    }
  }
  else
  {
    /// The file exists. Lets verify if it can be used immediately or rather shall be regenerated.
    if(load_header())
    {
      if(head_block_num < _header.head_block_num)
      {
        wlog("block_log file is shorten than current block_log.artifact file - the artifact file will be truncated.");
        /// Artifact file is too big. Let's try to truncate it
        truncate_file(head_block_num);
      }
      else
      if(head_block_num > _header.head_block_num)
      {
        wlog("block_log file is longer than current block_log.artifact file - artifact file generation will be resumed for range: <${fb}:${lb}>.",
        ("fb", _header.head_block_num + 1)("lb", head_block_num));
        /// Artifact file is too short - we need to resume its generation
        generate_file(source_block_provider, _header.head_block_num+1, head_block_num);
      }
      else
      {
        ilog("block_log and block_log.artifacts files match - no generation needed.");
      }
    }
    else
    {
      wlog("Block artifacts file ${filename} exists, but its header validation failed.", ("filename", _artifact_file_name));

      if(read_only)
      {
        FC_THROW("Generation of Block artifacts file ${filename} is disallowed by enforced read-only access.", ("filename", _artifact_file_name));
      }
      else
      {
        ilog("Attempting to overwrite existing artifacts file ${filename} exists...", ("filename", _artifact_file_name));

        HANDLE_IO((::close(_storage_fd)), "Closing the artifact file (before truncation)");

        _storage_fd = ::open(_artifact_file_name.generic_string().c_str(), flags|O_TRUNC, 0644);
        if(_storage_fd == -1)
          FC_THROW("Error creating block artifacts file ${filename}: ${error}", ("filename", _artifact_file_name)("error", strerror(errno)));

        _header = artifact_file_header();
        _header.dirty_close = 1;
        flush_header();

        generate_file(source_block_provider, 1, head_block_num);

        _header.head_block_num = head_block_num;
        flush_header();
      }
    }
  }
}

bool block_log_artifacts::impl::load_header()
{
  try
  {
    read_data(&_header, 0, "Reading the artifact file header");

    ilog("Loaded header containing: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}",
      ("gr", _header.git_version)("major", _header.format_major_version)("minor", _header.format_minor_version)("hb", _header.head_block_num));

    return true;
  }
  catch(const fc::exception& e)
  {
    wlog("Loading the artifact file header failed: ${e}", ("e", e.to_detail_string()));
    return false;
  }
}

void block_log_artifacts::impl::flush_header() const
{
  dlog("Header pack size: ${p}", ("p", header_pack_size));

  dlog("Attempting to write header containing: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}, dirty_close: ${d}",
    ("gr", _header.git_version)("major", _header.format_major_version)("minor", _header.format_minor_version)("hb", _header.head_block_num)("d", _header.dirty_close));

  write_data(_header, 0, "Flushing a file header");

  //artifact_file_header _h2(1);
  //ilog("header before read: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}, dirty_close: ${d}",
  //  ("gr", _h2.git_version)("major", _h2.format_major_version)("minor", _h2.format_minor_version)("hb", _h2.head_block_num)("d", _h2.dirty_close));

  //read_data(&_h2, 0, "Reading the artifact file header");

  //ilog("Loaded header containing: git rev: ${gr}, format version: ${major}.${minor}, head_block_num: ${hb}, dirty_close: ${d}",
  //  ("gr", _h2.git_version)("major", _h2.format_major_version)("minor", _h2.format_minor_version)("hb", _h2.head_block_num)("d", _h2.dirty_close));
}

void block_log_artifacts::impl::generate_file(const block_log& source_block_provider, uint32_t first_block, uint32_t last_block)
{
  ilog("Attempting to generate a block artifact file for block range: ${fb}...${lb}", ("fb", first_block)("lb", last_block));

  uint64_t time_begin = timestamp_ms();

  FC_ASSERT(first_block <= last_block, "${first_block} <= ${last_block}", ("first_block", first_block)("last_block", last_block));

  uint32_t block_count = last_block - first_block + 1;

  const uint32_t min_blocks_per_thread = 1000;
  const uint32_t thread_count = block_count > min_blocks_per_thread ? 4 : 1;

  const uint32_t blocks_per_thread = block_count / thread_count;
  
  uint32_t rest = block_count - blocks_per_thread*thread_count;

  std::vector<artifacts_t> artifacts_to_collect;
  artifacts_to_collect.reserve(blocks_per_thread);

  typedef std::pair<std::thread, std::future<worker_thread_result>> thread_data_t;

  std::map<uint32_t, thread_data_t> spawned_threads;

  source_block_provider.for_each_block_position(
    [this, first_block, last_block, blocks_per_thread, &source_block_provider, &artifacts_to_collect, &spawned_threads, &rest]
    (uint32_t block_num, uint32_t block_size, uint64_t block_pos, const block_attributes_t& block_attrs) -> bool
    {
      /// Since here is backward processing direction lets ignore and stop for all blocks excluded from requested range
      if(block_num < first_block)
        return false;

      if(block_num > last_block)
        return true;

      //dlog("Collecting data: ${b}, block_pos: ${block_pos}, block_size: ${block_size}", ("b", block_num)("block_pos", block_pos)("block_size", block_size));

      artifacts_to_collect.emplace_back(block_attrs, block_pos, block_size);

      /// First spawned thread will also include number of 'rest' blocks.
      if(artifacts_to_collect.size() == (static_cast<size_t>(blocks_per_thread) + rest))
      {
        rest = 0; /// clear rest to give further threads estimated amount of blocks.
        size_t block_info_count = artifacts_to_collect.size();

        ilog("Collected amount of block data sufficient to start a worker thread...");

        std::promise<worker_thread_result> work_promise;
        auto worker_thread_future = work_promise.get_future();

        auto emplace_info = spawned_threads.emplace(block_num, std::make_pair(
          std::thread(woker_thread_body, std::cref(source_block_provider), block_num, std::move(artifacts_to_collect), std::move(work_promise)),
            std::move(worker_thread_future)));

        FC_ASSERT(emplace_info.second);

        const thread_data_t& td = emplace_info.first->second;
        const std::thread& t = td.first;
        size_t tid = (size_t)const_cast<std::thread*>(&t);

        ilog("Spawned worker thread #${i} covering block range:<${fb}:${lb}>", ("i", tid)("fb", block_num)
          ("lb", block_num + block_info_count - 1));
      }

      return true;
    }
    );

  ilog("${tc} threads spawned - waiting for their work finish...", ("tc", spawned_threads.size()));

  for(auto& thread_info : spawned_threads)
  {
    std::thread& t = thread_info.second.first;
    size_t tid = (size_t)&t;

    try
    {
      ilog("Waiting for thread #${t} finish...", ("t", tid));

      thread_data_t& thread_data = thread_info.second;
      worker_thread_result result = thread_data.second.get();

      t.join();

      ilog("Thread #${t} finished...", ("t", tid));

      flush_data_chunks(result.first, result.second);
    }
    catch(const fc::exception& e)
    {
      ilog("Thread #${t} failed with exception: ${e}...", ("t", tid)("e", e.to_detail_string()));
      throw;
    }
    catch(const std::exception& e)
    {
      ilog("Thread #${t} failed with exception: ${e}...", ("t", tid)("e", e.what()));
      throw;
    }
  }

  ilog("All threads finished...");

  uint64_t time_end = timestamp_ms();
  auto elapsed_time = time_end - time_begin;

  ilog("Block artifact file generation finished. ${bc} blocks processed in time: ${t} ms", ("bc", block_count)("t", elapsed_time));
}

void block_log_artifacts::impl::flush_data_chunks(uint32_t min_block_num, const std::vector<artifacts_t>& data)
{
  uint64_t time_begin = timestamp_ms();
  
  uint32_t max_block_num = min_block_num + data.size() - 1;
  ilog("Attempting to flush ${s} artifact entries collected for block range: <${sb}:${eb}> into target file...",
    ("s", data.size())("sb", min_block_num)("eb", max_block_num));

  /// warning: data in artifacts container are placed in REVERSED order since block_log processing has backward direction
  uint32_t processed_block_num = min_block_num;
  for(auto dataI = data.rbegin(); dataI != data.rend(); ++dataI, ++processed_block_num)
  {
    store_block_artifacts(processed_block_num, dataI->block_log_file_pos, dataI->attributes, dataI->block_id);
  }

  /// STL has open ("past") end, so iterates once more
  --processed_block_num;

  FC_ASSERT(processed_block_num == max_block_num, "processed_block_num: ${pb} vs max_block_num: ${mb} mismatch", ("pb", processed_block_num)("mb", max_block_num));

  uint64_t time_end = timestamp_ms();
  auto elapsed_time = time_end - time_begin;

  ilog("Flushing ${s} artifact entries for block range: <${sb}:${eb}> finished in time: ${t} ms.",
    ("s", data.size())("sb", min_block_num)("eb", max_block_num)("t", elapsed_time));
}

void block_log_artifacts::impl::woker_thread_body(const block_log& block_provider, uint32_t min_block_num, std::vector<artifacts_t> data,
  std::promise<worker_thread_result> work_promise)
{
  try
  {
    uint32_t first_block_num = 0;
    for(auto artifactI = data.rbegin(); artifactI != data.rend(); ++artifactI)
    {
      auto& a = *artifactI;
      std::shared_ptr<full_block_type> full_block = block_provider.read_block_by_offset(a.block_log_file_pos, a.block_serialized_data_size,
        a.attributes);
      a.block_id = full_block->get_block_id();

      if(first_block_num == 0)
      {
        first_block_num = full_block->get_block_num();
        FC_ASSERT(first_block_num == min_block_num, "first_block: ${fb} != min_block: ${mb}", ("fb", first_block_num)("mb", min_block_num));
      }
    }

    work_promise.set_value(std::make_pair(min_block_num, std::move(data)));
  }
  catch(...)
  {
    try
    {
      auto eptr = std::current_exception();
      work_promise.set_exception(eptr);
    }
    catch(...)
    {
      elog("Storing exception ptr in the thread promise failed with another exception...");
    }
  }
}

void block_log_artifacts::impl::truncate_file(uint32_t last_block)
{
  auto last_chunk_position = calculate_offset(last_block);
  /// File truncate should be done just after last data chunk stored.
  auto truncate_position = last_chunk_position + artifact_chunk_size;

  if(ftruncate(_storage_fd, truncate_position) == -1)
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
  /// Nothing to do here, but definition must be placed here because of impl type visibility
}

block_log_artifacts::block_log_artifacts_ptr_t block_log_artifacts::open(const fc::path& block_log_file_path,
  bool read_only, const block_log& source_block_provider, uint32_t head_block_num)
{
  block_log_artifacts_ptr_t block_artifacts(new block_log_artifacts(),
    [](block_log_artifacts* bla) { impl::on_destroy(bla); });

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

      size_sum += prev_block_size;

      storage.back().block_serialized_data_size = prev_block_size;

      storage.emplace_back(chunk.unpack_data(block_num + i));
    }

    /// supplement last produced artifacts by block size
    prev_block_size = calculate_block_serialized_data_size(next_chunk, chunk_buffer[chunk_count - 1]);
    storage.back().block_serialized_data_size = prev_block_size;

    size_sum += prev_block_size;
  });

  if(block_size_sum != nullptr)
    *block_size_sum = size_sum;

  return storage;
}

void block_log_artifacts::store_block_artifacts(uint32_t block_num, uint64_t block_log_file_pos, const block_attributes_t& block_attributes,
  const block_id_t& block_id)
{
  _impl->store_block_artifacts(block_num, block_log_file_pos, block_attributes, block_id);
}

}}


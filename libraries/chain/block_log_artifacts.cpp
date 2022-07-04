#include <hive/chain/block_log_artifacts.hpp>

#include <hive/utilities/git_revision.hpp>
#include <hive/utilities/io_primitives.hpp>

#include <fc/io/raw.hpp>
#include <fc/bitutil.hpp>

#include <fcntl.h>

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
  std::string git_version = hive::utilities::git_revision_sha; /// version of a tool which constructed given file
  uint16_t format_major_version = FORMAT_MAJOR; /// version info of storage format (to allow potential upgrades in the future)
  uint16_t format_minor_version = FORMAT_MINOR;
  uint32_t head_block_num = 0; /// number of newest (head) block the file holds informations for
  bool dirty_close = true;
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
      uint64_t is_compressed : 1;
      uint64_t custom_dict_used : 1;
      uint64_t dictionary_no : 14;
      uint64_t block_log_offset : 48;
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

  void pack_data(uint64_t block_start_pos, block_attributes_t attributes)
  {
    combined_pos_and_attrs = detail::combine_block_start_pos_with_flags(block_start_pos, attributes);
    FC_ASSERT(is_compressed == (attributes.flags == block_flags::zstd));
    FC_ASSERT(custom_dict_used == attributes.dictionary_number.valid());
    FC_ASSERT(custom_dict_used == 0 || (dictionary_no == *attributes.dictionary_number));
    FC_ASSERT(block_start_pos == block_log_offset);
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
    FC_ASSERT(block_id._hash[0] == fc::endian_reverse_u32(block_num), "Malformed id: ${id} for block: ${b}", ("id", block_id)("b", block_num));

    memcpy(&stripped_block_id, block_id._hash + 1, sizeof(stripped_block_id));
  }

};

} /// anonymous


class block_log_artifacts::impl final
{
public:
  void try_to_open(const fc::path& block_log_file_path, bool read_only, const block_log& source_block_provider, uint32_t head_block_num);

  uint32_t read_head_block_num() const
  {
    return _header.head_block_num;
  }

  artifacts_t read_block_artifacts(uint32_t block_num) const;
  void store_block_artifacts(uint32_t block_num, const block_attributes_t& block_attributes, const block_id_t& block_id);

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

    _header.dirty_close = false;

    flush_header();

    HANDLE_IO((::close(_storage_fd)), "Closing the artifact file");

    _storage_fd = -1;

    ilog("Block log artifact file closed.");
  }

private:
  bool load_header();
  void flush_header();

  void generate_file(const block_log& source_block_provider, uint32_t first_block, uint32_t last_block);
  void truncate_file(uint32_t last_block);

  void write_data(const std::vector<char>& buffer, off_t offset, const std::string& description)
  {
    hive::utilities::perform_write(_storage_fd, buffer.data(), buffer.size(), offset, description);
  }

  template <class Data, unsigned int N=1>
  void write_data(const Data& buffer, off_t offset, const std::string& description)
  {
    hive::utilities::perform_write(_storage_fd, reinterpret_cast<const char*>(&buffer), N*sizeof(Data), offset, description);
  }

  std::vector<char> read_data(size_t to_read, off_t offset, const std::string& description) const
  {
    std::vector<char> buffer;
    buffer.resize(to_read);

    auto total_read = hive::utilities::perform_read(_storage_fd, buffer.data(), to_read, offset, description);

    FC_ASSERT(total_read == to_read, "Incomplete read: expected: ${r}, performed: ${tr}", ("r", to_read)("tr", total_read));

    return buffer;
  }

  template <class Data, unsigned int N=1>
  void read_data(Data* buffer, off_t offset, const std::string& description) const
  {
    const auto to_read = N*sizeof(Data);
    auto total_read = hive::utilities::perform_read(_storage_fd, reinterpret_cast<char*>(buffer), to_read, offset, description);

    FC_ASSERT(total_read == to_read, "Incomplete read: expected: ${r}, performed: ${tr}", ("r", to_read)("tr", total_read));
  }

  size_t calculate_offset(uint32_t block_num) const
  {
    return header_pack_size + artifact_chunk_size*(block_num - 1);
  }

private:
  fc::path _artifact_file_name;
  int _storage_fd = -1; /// file descriptor to the opened file.
  artifact_file_header _header;
  const size_t header_pack_size = fc::raw::pack_size(_header);
  const size_t artifact_chunk_size = sizeof(artifact_file_chunk);
  bool _is_writable = false;
};

void block_log_artifacts::impl::try_to_open(const fc::path& block_log_file_path, bool read_only,
  const block_log& source_block_provider, uint32_t head_block_num)
{
  _artifact_file_name = fc::path(block_log_file_path.generic_string() + ".artifacts");
  _is_writable = read_only == false;

  int flags = O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC;
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
        _storage_fd = ::open(_artifact_file_name.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
        if(_storage_fd == -1)
          FC_THROW("Error creating block artifacts file ${filename}: ${error}", ("filename", _artifact_file_name)("error", strerror(errno)));

        generate_file(source_block_provider, 1, head_block_num);
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
        /// Artifact file is too big. Let's try to truncate it
        truncate_file(head_block_num);
      }
      else
      if(head_block_num > _header.head_block_num)
      {
        /// Artifact file is too short - we need to resume its generation
        generate_file(source_block_provider, _header.head_block_num+1, head_block_num);
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

        generate_file(source_block_provider, 1, head_block_num);
      }
    }
  }
}

bool block_log_artifacts::impl::load_header()
{
  try
  {
    std::vector<char> buffer = read_data(header_pack_size, 0, "Reading the artifact file header");
    FC_ASSERT(buffer.size() == header_pack_size);

    fc::raw::unpack_from_vector(buffer, _header);

    return true;
  }
  catch(const fc::exception& e)
  {
    elog("Loading the artifact file header failed: ${e}", ("e", e.to_detail_string()));
    return false;
  }
}

void block_log_artifacts::impl::flush_header()
{
  std::vector<char> buffer;

  buffer = fc::raw::pack_to_vector(_header);

  FC_ASSERT(buffer.size() == header_pack_size);

  write_data(buffer, 0, "Flushing a file header");
}

void block_log_artifacts::impl::generate_file(const block_log& source_block_provider, uint32_t first_block, uint32_t last_block)
{
  ilog("Attempting to generate a block artifact file...");

  ilog("Block artifact file generation finished...");
}

void block_log_artifacts::impl::truncate_file(uint32_t last_block)
{
  auto last_chunk_position = calculate_offset(last_block);
  /// File truncate should be done just after last data chunk stored.
  auto truncate_position = last_chunk_position + artifact_chunk_size;

  if(ftruncate(_storage_fd, truncate_position) == -1)
    FC_THROW("Error truncating block artifact file: ${error}", ("error", strerror(errno)));
}

block_log_artifacts::artifacts_t block_log_artifacts::impl::read_block_artifacts(uint32_t block_num) const
{
  artifacts_t artifacts;

  auto chunk_position = calculate_offset(block_num);

  artifact_file_chunk data_chunk;



  return artifacts;
}

void block_log_artifacts::impl::store_block_artifacts(uint32_t block_num, const block_attributes_t& block_attrs,
  const block_id_t& block_id)
{

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
  return _impl->read_block_artifacts(block_num);
}

void block_log_artifacts::store_block_artifacts(uint32_t block_num, const block_attributes_t& block_attributes, const block_id_t& block_id)
{
  _impl->store_block_artifacts(block_num, block_attributes, block_id);
}

}}

FC_REFLECT(hive::chain::artifact_file_header, (git_version)(format_major_version)(format_minor_version)(head_block_num)(dirty_close));


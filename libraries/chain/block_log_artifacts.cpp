#include <hive/chain/block_log_artifacts.hpp>

#include <hive/utilities/git_revision.hpp>

namespace hive { namespace chain {

namespace { /// anonymous

const uint16_t FORMAT_MAJOR = 1;
const uint16_t FORMAT_MINOR = 1;

struct artifact_file_header
{
  std::string git_version = hive::utilities::git_revision_sha; /// version of a tool which constructed given file
  uint16_t format_major_version = FORMAT_MAJOR; /// version info of storage format (to allow potential upgrades in the future)
  uint16_t format_minor_version = FORMAT_MINOR;
  uint32_t head_block_num = 0; /// number of newest (head) block the file holds informations for
};

} /// anonymous


class block_log_artifacts::impl final
{
public:
  void try_to_open(const fc::path& block_log_file_path, bool read_only);

  uint32_t read_head_block_num() const
  {
    return _head_block_num;
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
    ilog("Closing a block log artifact file: ${1} file...", (_artifact_file_name.generic_string()));

    ::close(_storage_fd);
    _storage_fd = -1;

    ilog("Block log artifact file closed.");
  }

private:
  fc::path _artifact_file_name;
  int _storage_fd = -1; /// file descriptor to the opened file.
  uint32_t _head_block_num = 0;
  bool _is_writable = false;
};

void block_log_artifacts::impl::try_to_open(const fc::path& block_log_file_path, bool read_only)
{
  _artifact_file_name = fc::path(block_log_file_path.generic_string() + ".artifacts");
  _is_writable = read_only == false;



}

block_log_artifacts::artifacts_t block_log_artifacts::impl::read_block_artifacts(uint32_t block_num) const
{
  artifacts_t artifacts;

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
  bool read_only)
{
  block_log_artifacts_ptr_t block_artifacts(new block_log_artifacts(),
    [](block_log_artifacts* bla) { impl::on_destroy(bla); });

  block_artifacts->_impl->try_to_open(block_log_file_path, read_only);

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

FC_REFLECT(hive::chain::artifact_file_header, (git_version)(format_major_version)(format_minor_version)(head_block_num));


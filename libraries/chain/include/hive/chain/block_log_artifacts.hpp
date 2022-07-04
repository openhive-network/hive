#pragma once

#include <hive/protocol/types.hpp>

#include <hive/chain/detail/block_attributes.hpp>

#include <fc/filesystem.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace hive { namespace chain {

class block_log;

/** This class is responsible for managing (migration, reading, writing) additional data to block_log file itself like:
* - block_log fileoffset for given block number (useful to quickly access block by number)
* - block storage flags, determining if block is stored as compressed etc
* - holding a block hash what can eliminate often hash calculation
*   
*   Notes related to storage format:
* 
*   Artifact file starts with header holding few additional properties useful to perform satity checks etc.
*   Any file having no valid header will be considered as broken one and regenerated if possible. 
*
*/
class block_log_artifacts final
{
public:
  typedef std::unique_ptr<block_log_artifacts, std::function<void(block_log_artifacts*)> > block_log_artifacts_ptr_t;

  using block_digest_t= hive::protocol::digest_type;
  using block_id_t    = hive::protocol::block_id_type;

  using block_attributes_t = hive::chain::detail::block_attributes_t;

  struct artifacts_t
  {
    block_attributes_t attributes;
    block_id_t block_id;
    uint64_t   block_log_file_pos;
  };

  /** Allows to open a block log aartifacts file located in the same directory as specified block_log file itself.
  *   \param block_log_file_path location of source block_log file
  *   \param read_only - if set, already existing artifacts file must match to pointed block log.
  *   \param source_block_provider - provides a block data to generate artifact file.
  *   \param head_block_num - max block number (head block) contained by given block_log.
  *   Built instance of `block_log_artifacts` will be automaticaly closed before destruction.
  * 
  *   Function throws on any error f.e. related to IO.
  */
  static block_log_artifacts_ptr_t open(const fc::path& block_log_file_path, bool read_only,
    const block_log& source_block_provider, uint32_t head_block_num);

  /// Allows to read a number of last block the artifacts are stored for.
  uint32_t read_head_block_num() const;

  artifacts_t read_block_artifacts(uint32_t block_num) const;

  void store_block_artifacts(uint32_t block_num, const block_attributes_t& block_attributes, const block_id_t& block_id);

private:
  class impl;

  block_log_artifacts();
  ~block_log_artifacts();

  block_log_artifacts(const block_log_artifacts&) = delete;
  block_log_artifacts(block_log_artifacts&&) = delete;
  block_log_artifacts& operator=(const block_log_artifacts&) = delete;
  block_log_artifacts& operator=(block_log_artifacts&&) = delete;

/// Class attributes:
private:
  std::unique_ptr<impl> _impl;
};

} }



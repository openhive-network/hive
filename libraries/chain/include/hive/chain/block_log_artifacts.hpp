#pragma once

#include <hive/protocol/types.hpp>

#include <hive/chain/detail/block_attributes.hpp>

#include <fc/filesystem.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

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
  typedef std::unique_ptr<block_log_artifacts> block_log_artifacts_ptr_t;

  using block_digest_t= hive::protocol::digest_type;
  using block_id_t    = hive::protocol::block_id_type;

  using block_attributes_t = hive::chain::detail::block_attributes_t;

  struct artifacts_t
  {
    artifacts_t(const block_attributes_t& attrs, uint64_t file_pos, uint32_t block_size) :
      attributes(attrs), block_log_file_pos(file_pos), block_serialized_data_size(block_size) {}
    artifacts_t() = default;
    artifacts_t(artifacts_t&&) = default;
    artifacts_t& operator=(artifacts_t&&) = default;

    block_attributes_t attributes;
    block_id_t block_id;
    uint64_t   block_log_file_pos = 0;
    uint32_t   block_serialized_data_size = 0;
  };

  typedef std::vector<artifacts_t> artifact_container_t;

  ~block_log_artifacts();

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

  /**
   * @brief Allows to query for artifacts of blocks pointed by particular block range.
   * 
   * @param start_block_num - input, first block to read artifacts for
   * @param block_count     - input, number of blocks to be processed
   * @param block_size_sum  - output, optional - allows to retrieve total size of raw block storage, to simplify their load from block_log file.
   *                          \warning This sum additionally includes sizeof of additional field stored after each block (prev-block-position)
                              to simplify reading data for multiple blocks at one time. This field should be accordingly skipped in the memory buffer
                              while accessing RAW block data.
   * @return filled container holding artifacts for queried blocks. First stored item is specific to `start_block_num`, next are following them.
  */
  artifact_container_t read_block_artifacts(uint32_t start_block_num, uint32_t block_count, size_t* block_size_sum = nullptr) const;

  void store_block_artifacts(uint32_t block_num, uint64_t block_log_file_pos, const block_attributes_t& block_attributes,
                             const block_id_t& block_id);

  void truncate(uint32_t new_head_block_num);
private:
  class impl;

  block_log_artifacts();

  block_log_artifacts(const block_log_artifacts&) = delete;
  block_log_artifacts(block_log_artifacts&&) = delete;
  block_log_artifacts& operator=(const block_log_artifacts&) = delete;
  block_log_artifacts& operator=(block_log_artifacts&&) = delete;

/// Class attributes:
private:
  std::unique_ptr<impl> _impl;
};

} }



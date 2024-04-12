#pragma once

#include <hive/chain/split_file_block_log_writer.hpp>

namespace hive { namespace chain {

  /**
   * @brief Pruned variant of multi-file implementation of block_log_writer_common.
   * Allows the first N part files of the block log to be missing - which makes blocks 
   * [1:N*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE] unavailable.
   * 
   * Note: At least one full part file (at least BLOCKS_IN_SPLIT_BLOCK_LOG_FILE of last blocks)
   *       is required to be present.
   */
  class pruned_block_log_writer : public split_file_block_log_writer
  {
  protected:
    virtual uint32_t validate_tail_part_number( uint32_t tail_part_number, uint32_t head_part_number ) const override;
    virtual void rotate_part_files( uint32_t new_part_number ) override;

  public:
    pruned_block_log_writer( uint32_t split_file_kept, appbase::application& app,
                             blockchain_worker_thread_pool& thread_pool );
    virtual ~pruned_block_log_writer() = default;

    /// Unavailable with pruning.
    virtual std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> read_raw_block_data_by_num(uint32_t block_num) const override;

  private:
    uint32_t  _split_file_kept; /// As specified in config file.
  };

} }

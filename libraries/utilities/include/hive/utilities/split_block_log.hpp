#pragma once

#include <appbase/application.hpp>

#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <fc/filesystem.hpp>

namespace hive { namespace utilities {

/**
 * @brief Tries splitting provided legacy block log file into parts.
 * 
 * @param monolith_path to log file. The split parts go into its directory.
 * @param head_part_number highest number of part file to be generated, Deduct from source file if 0.
 * @param part_count how many part files shall be generated. All if 0.
 * @param splitted_block_log_destination_dir directory where new style multiple block_log files will be stored. By default it's the same place where monolith block_log is.
 * @throws lots of FC_ASSERTs.
 */
void split_block_log( fc::path monolith_path, uint32_t head_part_number, size_t part_count,
  appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool,
  const fc::optional<fc::path> splitted_block_log_destination_dir = fc::optional<fc::path>() );

} } // hive::utilities

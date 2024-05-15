#pragma once

#include <appbase/application.hpp>

#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <fc/filesystem.hpp>

namespace hive { namespace utilities {

/**
 * @brief Tries splitting provided legacy block log file into parts.
 * 
 * @param monolith_path to log file. The split parts go into its directory.
 * @throws lots of FC_ASSERTs.
 */
void split_block_log( fc::path monolith_path, appbase::application& app,
                      hive::chain::blockchain_worker_thread_pool& thread_pool );

} } // hive::utilities

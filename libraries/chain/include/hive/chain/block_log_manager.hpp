#pragma once

#include <hive/chain/block_log_writer_common.hpp>

#include <fc/filesystem.hpp>

#include <memory>

#define LEGACY_SINGLE_FILE_BLOCK_LOG -1
#define MULTIPLE_FILES_FULL_BLOCK_LOG 0

namespace appbase {
  class application;
}
namespace hive { namespace chain {

class blockchain_worker_thread_pool;
using appbase::application;

class block_log_manager_t final
{
public:
  using block_log_reader_t = std::shared_ptr< block_log_reader_common >;
  using block_log_writer_t = std::shared_ptr< block_log_writer_common >;

  static block_log_writer_t create_writer( int block_log_split,
    appbase::application& app, blockchain_worker_thread_pool& thread_pool );

  static block_log_reader_t create_opened_reader( const fc::path& input_path,
    appbase::application& app, blockchain_worker_thread_pool& thread_pool,
    bool recreate_artifacts_if_needed = true );

  static block_log_writer_t create_opened_writer( const fc::path& output_path,
    appbase::application& app, blockchain_worker_thread_pool& thread_pool );
};

} }
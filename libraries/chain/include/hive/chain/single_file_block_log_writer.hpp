#pragma once

#include <hive/chain/block_log.hpp>
#include <hive/chain/block_log_writer_common.hpp>

namespace appbase {
  class application;
}
namespace hive { namespace chain {

  /// Legacy single-file implementation of block_log_writer_common.
  class single_file_block_log_writer : public block_log_writer_common
  {
  protected:
    /// Required by block_log_reader_common.
    virtual const block_log& get_head_block_log() const override;

    /// Required by block_log_reader_common.
    virtual const block_log* get_block_log_corresponding_to( uint32_t block_num ) const override;

    /// Required by block_log_reader_common.
    virtual block_range_t read_block_range_by_num( uint32_t starting_block_num, uint32_t count ) const override;

    /// Required by block_log_writer_common.
    virtual block_log& get_head_block_log() override;

    /// Required by block_log_writer_common.
    virtual block_log& get_log_for_new_block() override;

  public:
    single_file_block_log_writer( appbase::application& app );
    virtual ~single_file_block_log_writer() = default;

    /// Required by block_log_writer_common.
    virtual void open_and_init( const block_log_open_args& bl_open_args,
                                 blockchain_worker_thread_pool& thread_pool ) override;

    /// The only method of block_read_i not implemented by block_log_reader_common.
    virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor, hive::chain::blockchain_worker_thread_pool& thread_pool ) const override;

  private:
    block_log _block_log;
  };

} }

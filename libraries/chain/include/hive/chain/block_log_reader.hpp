#pragma once

#include <hive/chain/block_log_reader_common.hpp>

namespace hive { namespace chain {

  class block_log;

  class block_log_reader : public block_log_reader_common
  {
  protected:
    /// Required by block_log_reader_common.
    virtual const block_log& get_head_block_log() const;

    /// Required by block_log_reader_common.
    virtual const block_log* get_block_log_corresponding_to( uint32_t block_num ) const;

    /// Required by block_log_reader_common.
    virtual block_range_t read_block_range_by_num( uint32_t starting_block_num, uint32_t count ) const;

  public:
    block_log_reader( const block_log& the_log );
    virtual ~block_log_reader() = default;

    virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor, hive::chain::blockchain_worker_thread_pool& thread_pool ) const override;

  private:
    const block_log& _block_log;
  };

} }

#pragma once

#include <hive/chain/block_read_interface.hpp>

namespace hive { namespace chain {

  class block_log;

  class block_log_reader : public block_read_i
  {
  public:
    block_log_reader( block_log& block_log );
    virtual ~block_log_reader() = default;

    virtual std::shared_ptr<full_block_type> head() const override;

    virtual std::shared_ptr<full_block_type> read_block_by_num( uint32_t block_num ) const override;

    virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor ) override;

  private:
    block_log&      _block_log;
  };

} }

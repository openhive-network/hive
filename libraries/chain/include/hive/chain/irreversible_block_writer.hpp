#pragma once

#include <hive/chain/block_write_interface.hpp>

#include <hive/chain/block_log_reader.hpp>

namespace hive { namespace chain {

  class block_log;

  class irreversible_block_writer : public block_write_i
  {
  public:
    irreversible_block_writer( const block_log& block_log );
    virtual ~irreversible_block_writer() = default;

    virtual block_read_i& get_block_reader() override;

    /**
     * 
     */
    virtual void store_block( uint32_t current_irreversible_block_num,
                              uint32_t state_head_block_number ) override;

    virtual void pop_block() override { FC_ASSERT( false, "Wrong writer bro" ); }

  private:
    block_log_reader _reader;
  };

} }

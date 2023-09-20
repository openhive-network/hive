#pragma once

#include <hive/chain/block_write_interface.hpp>

namespace hive { namespace chain {

  class block_log;
  class fork_database;

  class irreversible_block_writer : public block_write_i
  {
  public:
    irreversible_block_writer( block_log& block_log, fork_database& fork_db );
    virtual ~irreversible_block_writer() {}

    virtual block_log& get_block_log() override { return _block_log; };
    virtual fork_database& get_fork_db() override { return _fork_db; };

    virtual void set_is_at_live_sync() override { FC_ASSERT( false, "Wrong writer bro" ); }

    /**
     * 
     */
    virtual void store_block( uint32_t current_irreversible_block_num,
                              uint32_t state_head_block_number ) override;

  private:
    block_log&      _block_log;
    fork_database&  _fork_db;
  };

} }

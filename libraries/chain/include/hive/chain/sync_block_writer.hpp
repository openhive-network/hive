#pragma once

#include <hive/chain/block_write_interface.hpp>

#include <hive/chain/fork_db_block_reader.hpp>

namespace hive { namespace chain {

  class block_log;
  class fork_database;

  class sync_block_writer : public block_write_i
  {
  public:
    sync_block_writer( block_log& block_log, fork_database& fork_db );
    virtual ~sync_block_writer() = default;

    virtual fork_database& get_fork_db() override { return _fork_db; };

    virtual block_read_i& get_block_reader() override;

    virtual void set_is_at_live_sync() override { _is_at_live_sync = true; }

    /**
     * 
     */
    virtual void store_block( uint32_t current_irreversible_block_num,
                              uint32_t state_head_block_number ) override;

    virtual void pop_block() override;  

    virtual void switch_forks( const block_id_type& new_head_block_id, uint32_t new_head_block_num,
      uint32_t skip, const block_flow_control* pushed_block_ctrl,
      const block_id_type original_head_block_id, const uint32_t original_head_block_number,
      apply_block_t apply_block_extended, pop_block_t pop_block_extended, 
      notify_switch_fork_t notify_switch_fork ) override;

  private:
    fork_db_block_reader  _reader;
    block_log&            _block_log;
    fork_database&        _fork_db;
    bool                  _is_at_live_sync = false;
  };

} }

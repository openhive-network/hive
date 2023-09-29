#pragma once

#include <hive/chain/block_read_interface.hpp>

#include <cstdint>

namespace hive { namespace chain {

  class block_log;
  class fork_database;

  class block_write_i
  {
  public:
  virtual ~block_write_i() = default;

  // Temporary, to be removed
  virtual fork_database& get_fork_db() = 0;

  virtual block_read_i& get_block_reader() = 0;

  /// Call on interested writer when hived goes into live sync.
  virtual void set_is_at_live_sync() = 0;

  /**
	 * Check that fork head (head block of the longest fork) matches state head.
	 * Update block log up to current (not old) LIB (excluding) with the reversible blocks on main branch.
	 * Trim new irreversible blocks from reversible main branch.
   */
  virtual void store_block( uint32_t current_irreversible_block_num,
                            uint32_t state_head_block_number ) = 0;

  virtual void pop_block() = 0;  
  };

} }

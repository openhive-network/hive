#pragma once

#include <hive/chain/block_read_interface.hpp>

#include <cstdint>

namespace hive { namespace chain {

  class block_write_i : public block_read_i
  {
  public:
	/**
	 * Check that fork head (head block of the longest fork) matches state head.
	 * Update block log up to current (not old) LIB (excluding) with the reversible blocks on main branch.
	 * Trim new irreversible blocks from reversible main branch.
	 * Drop undo sessions up to current (not old) LIB (including).
	 * Notify registered callback about irreversibility of the blocks up to current (not old) LIB (including).
	 */
	virtual void migrate_irreversible_state(uint32_t old_last_irreversible) = 0;
  };

} }

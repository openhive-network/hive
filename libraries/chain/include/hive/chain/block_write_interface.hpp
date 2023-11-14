#pragma once

#include <hive/chain/block_read_interface.hpp>

#include <cstdint>

namespace hive { namespace chain {

  class block_flow_control;
  class witness_object;

  using hive::protocol::account_name_type;

  class block_write_i
  {
  protected:
    virtual ~block_write_i() = default;

  public:
    virtual block_read_i& get_block_reader() = 0;

    /**
     * Check that fork head (head block of the longest fork) matches state head.
     * Update block log up to current (not old) LIB (excluding) with the reversible blocks on main branch.
     * Trim new irreversible blocks from reversible main branch.
     */
    virtual void store_block( uint32_t current_irreversible_block_num,
                              uint32_t state_head_block_number ) = 0;

    /** 
     * Do whatever is necessary when database is about to pop a reversible block from current
     * main branch during e.g. forks switching.
     */
    virtual void pop_block() = 0;  
  }; // class block_write_i

} } // namespace hive { namespace chain

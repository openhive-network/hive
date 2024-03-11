#pragma once

#include <hive/chain/block_read_interface.hpp>

#include <cstdint>

namespace hive { namespace chain {

  class witness_object;

  using hive::protocol::account_name_type;

  class block_write_i
  {
  protected:
    virtual ~block_write_i() = default;

  public:
    virtual const block_read_i& get_block_reader() = 0;

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

    struct new_last_irreversible_block_t
    {
      uint32_t new_last_irreversible_block_num = 0;
      bool found_on_another_fork = false;
      std::shared_ptr<full_block_type> new_head_block;
    };

    /**
     * @brief Using provided data find new LIB (last irreversible block).
     * @return the info on new LIB or empty optional if not found
     */
    virtual std::optional<new_last_irreversible_block_t> find_new_last_irreversible_block(
      const std::vector<const witness_object*>& scheduled_witness_objects,
      const std::map<account_name_type, block_id_type>& last_fast_approved_block_by_witness,
      const unsigned witnesses_required_for_irreversiblity,
      const uint32_t old_last_irreversible ) const = 0;
  }; // class block_write_i

} } // namespace hive { namespace chain

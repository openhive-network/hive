#pragma once

#include <hive/chain/block_write_interface.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace hive { namespace chain {

  class full_block_type;
  class witness_object;

  using hive::protocol::account_name_type;

  class last_irreversible_block_i
  {
  protected:
    virtual ~last_irreversible_block_i() = default;

  public:
    /**
     * 
     */
    virtual bool is_fork_switching_enabled( 
      std::shared_ptr<full_block_type> new_head_block,
      uint32_t old_last_reversible_block_num, 
      std::optional<uint32_t>& new_last_irreversible_block_num ) = 0;

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
  }; // class last_irreversible_block_i

} } // namespace hive { namespace chain

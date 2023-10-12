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

    virtual void set_is_at_live_sync() override { FC_ASSERT( false, "Wrong writer bro" ); }

    /**
     * 
     */
    virtual void store_block( uint32_t current_irreversible_block_num,
                              uint32_t state_head_block_number ) override;

    virtual void pop_block() override { FC_ASSERT( false, "Wrong writer bro" ); }

    virtual bool push_block(const std::shared_ptr<full_block_type>& full_block,
      const block_flow_control& block_ctrl, uint32_t state_head_block_num,
      block_id_type state_head_block_id, const uint32_t skip, apply_block_t apply_block_extended,
      pop_block_t pop_block_extended ) override
    { FC_ASSERT( false, "Wrong writer bro" ); }

    virtual void switch_forks( const block_id_type& new_head_block_id, uint32_t new_head_block_num,
      uint32_t skip, const block_flow_control* pushed_block_ctrl,
      const block_id_type original_head_block_id, const uint32_t original_head_block_number,
      apply_block_t apply_block_extended, pop_block_t pop_block_extended ) override
      { FC_ASSERT( false, "Wrong writer bro" ); }

    virtual std::optional<new_last_irreversible_block_t> find_new_last_irreversible_block(
      const std::vector<const witness_object*>& scheduled_witness_objects,
      const std::map<account_name_type, block_id_type>& last_fast_approved_block_by_witness,
      const unsigned witnesses_required_for_irreversiblity,
      const uint32_t old_last_irreversible ) const override
      { FC_ASSERT( false, "Wrong writer bro" ); }

  private:
    block_log_reader _reader;
  };

} }

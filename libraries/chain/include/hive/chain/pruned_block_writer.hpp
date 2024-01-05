#pragma once

#include <hive/chain/block_write_chain_interface.hpp>
#include <hive/chain/block_read_interface.hpp>

#include <appbase/application.hpp>

namespace hive { namespace chain {

  class database;
  class fork_database;
  class full_block_object;
  using appbase::application;

  class pruned_block_writer : public block_write_chain_i, public block_read_i
  {
  public:
    pruned_block_writer( uint16_t stored_block_number, 
      database& db, application& app, fork_database& fork_db );
    virtual ~pruned_block_writer() = default;

    // ### block_write_chain_i overrides ###
    virtual void set_is_at_live_sync() override {}
    virtual void on_reindex_start() override;
    virtual void on_reindex_end( const std::shared_ptr<full_block_type>& end_block ) override;
    virtual bool push_block(const std::shared_ptr<full_block_type>& full_block,
      const block_flow_control& block_ctrl, uint32_t state_head_block_num,
      block_id_type state_head_block_id, const uint32_t skip, apply_block_t apply_block_extended,
      pop_block_t pop_block_extended ) override;
    virtual void switch_forks( const block_id_type& new_head_block_id, uint32_t new_head_block_num,
      uint32_t skip, const block_flow_control* pushed_block_ctrl,
      const block_id_type original_head_block_id, const uint32_t original_head_block_number,
      apply_block_t apply_block_extended, pop_block_t pop_block_extended ) override;

    // ### block_write_i overrides ###
    virtual block_read_i& get_block_reader() override { return *this; };
    virtual void store_block( uint32_t current_irreversible_block_num,
                              uint32_t state_head_block_number ) override;
    virtual void pop_block() override;
    virtual std::optional<new_last_irreversible_block_t> find_new_last_irreversible_block(
      const std::vector<const witness_object*>& scheduled_witness_objects,
      const std::map<account_name_type, block_id_type>& last_fast_approved_block_by_witness,
      const unsigned witnesses_required_for_irreversiblity,
      const uint32_t old_last_irreversible ) const override;

    // ### block_read_i overrides ###
    virtual uint32_t head_block_num( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;
    virtual block_id_type head_block_id( 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;

    virtual std::shared_ptr<full_block_type> read_block_by_num( uint32_t block_num ) const override;

    virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor,
                                 hive::chain::blockchain_worker_thread_pool& thread_pool ) const override;

    virtual std::shared_ptr<full_block_type> fetch_block_by_number( uint32_t block_num,
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;

    virtual std::shared_ptr<full_block_type> fetch_block_by_id( 
      const block_id_type& id ) const override;

    virtual bool is_known_block( const block_id_type& id ) const override;

    virtual std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(
      const std::deque<block_id_type>& item_hashes_received ) const override;

    virtual full_block_vector_t fetch_block_range( 
      const uint32_t starting_block_num, const uint32_t count, 
      fc::microseconds wait_for_microseconds = fc::microseconds() ) const override;

    virtual std::vector<block_id_type> get_blockchain_synopsis(
      const block_id_type& reference_point, 
      uint32_t number_of_blocks_after_reference_point ) const override;

    virtual std::vector<block_id_type> get_block_ids(
      const std::vector<block_id_type>& blockchain_synopsis,
      uint32_t& remaining_item_count,
      uint32_t limit) const override;

  private:
    bool is_known_irreversible_block( const block_id_type& id ) const;
    void store_full_block( const std::shared_ptr<full_block_type> full_block );
    std::shared_ptr<full_block_type> retrieve_full_block( uint32_t recent_block_num ) const;
    const full_block_object* find_full_block( uint32_t recent_block_num ) const;
    full_block_vector_t get_block_range( uint32_t starting_block_num, uint32_t count ) const;
    block_id_type get_block_id_for_num( uint32_t block_num ) const;

  private:
    uint16_t        _stored_block_number;
    fork_database&  _fork_db;
    database&       _db;
    application&    _app; /// Needed only for notification purposes.
  };

} }

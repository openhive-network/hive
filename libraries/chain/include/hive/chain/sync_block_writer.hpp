#pragma once

#include <hive/chain/block_write_chain_interface.hpp>

#include <hive/chain/fork_db_block_reader.hpp>

#include <hive/chain/database.hpp>
#include <appbase/application.hpp>

namespace hive { namespace chain {

  class block_log_writer_common;
  class fork_database;
  using appbase::application;

  class sync_block_writer : public block_write_chain_i
  {
  public:
    sync_block_writer( block_log_writer_common& blw, fork_database& fdb, database& db, application& app );
    virtual ~sync_block_writer() = default;

    virtual const block_read_i& get_block_reader() override;

    virtual void store_block( uint32_t current_irreversible_block_num,
                              uint32_t state_head_block_number ) override;

    virtual void pop_block() override;

    virtual std::optional<new_last_irreversible_block_t> find_new_last_irreversible_block(
      const std::vector<const witness_object*>& scheduled_witness_objects,
      const std::map<account_name_type, block_id_type>& last_fast_approved_block_by_witness,
      const unsigned witnesses_required_for_irreversiblity,
      const uint32_t old_last_irreversible ) const override;

    void set_is_at_live_sync() { _is_at_live_sync = true; }

    using apply_block_t = std::function<
      void ( const std::shared_ptr< full_block_type >& full_block,
            uint32_t skip, const block_flow_control* block_ctrl ) >;
    /// Returns number of block on head after popping.
    using pop_block_t = std::function< uint32_t ( const block_id_type end_block ) >;

    /**
     * @brief A new block arrived and is being pushed into state. Check whether it extends current
     *        fork or creates another (longer one). Switch forks if necessary. Call provided
     *        callbacks to apply or pop any block in the process.
     * @param full_block the new block
     * @param block_ctrl use to report appropriate events
     * @param state_head_block_num refers to head block as stored in state
     * @param state_head_block_id refers to head block as stored in state
     * @param skip flags to be passed to apply block callback
     * @param apply_block_extended call when trying to build a fork
     * @param pop_block_extended call when trying to rewind a fork
     * @return true if the forks have been switched as a result of this push.
     */
    bool push_block(const std::shared_ptr<full_block_type>& full_block,
      const block_flow_control& block_ctrl, uint32_t state_head_block_num,
      block_id_type state_head_block_id, const uint32_t skip, apply_block_t apply_block_extended,
      pop_block_t pop_block_extended );

    /**
     * @brief Used during transaction fast confirmation.
     *        Other than that a part of push_block implementation.
     * @param new_head_block_id id of the new (fork) head candidate
     * @param new_head_block_num num of the new (fork) head candidate
     * @param skip flags to be passed to apply block callback
     * @param pushed_block_ctrl use to report appropriate events
     * @param original_head_block_id id of current/former (fork) head
     * @param original_head_block_number num of current/former (fork) head
     * @param apply_block_extended call when trying to build a fork
     * @param pop_block_extended call when trying to rewind a fork
     */
    void switch_forks( const block_id_type& new_head_block_id, uint32_t new_head_block_num,
      uint32_t skip, const block_flow_control* pushed_block_ctrl,
      const block_id_type original_head_block_id, const uint32_t original_head_block_number,
      apply_block_t apply_block_extended, pop_block_t pop_block_extended );

  private:
    block_log_writer_common&  _log_writer;
    fork_database&            _fork_db;
    fork_db_block_reader      _reader;
    bool                      _is_at_live_sync = false;
    database&                 _db; /// Needed only for notification purposes.
    application&              _app; /// Needed only for notification purposes.
  };

} }

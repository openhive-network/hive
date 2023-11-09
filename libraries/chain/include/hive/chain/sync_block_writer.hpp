#pragma once

#include <hive/chain/block_write_interface.hpp>

#include <hive/chain/fork_db_block_reader.hpp>

#include <hive/chain/database.hpp>
#include <appbase/application.hpp>

namespace hive { namespace chain {

  class block_log;
  class fork_database;
  using appbase::application;

  class sync_block_writer : public block_write_i
  {
  public:
    sync_block_writer( database& db, application& app );
    virtual ~sync_block_writer() = default;

    virtual block_read_i& get_block_reader();

    virtual void set_is_at_live_sync() override { _is_at_live_sync = true; }

    virtual void store_block( uint32_t current_irreversible_block_num,
                              uint32_t state_head_block_number ) override;

    virtual void pop_block() override;

    virtual bool push_block(const std::shared_ptr<full_block_type>& full_block,
      const block_flow_control& block_ctrl, uint32_t state_head_block_num,
      block_id_type state_head_block_id, const uint32_t skip, apply_block_t apply_block_extended,
      pop_block_t pop_block_extended ) override;

    virtual void switch_forks( const block_id_type& new_head_block_id, uint32_t new_head_block_num,
      uint32_t skip, const block_flow_control* pushed_block_ctrl,
      const block_id_type original_head_block_id, const uint32_t original_head_block_number,
      apply_block_t apply_block_extended, pop_block_t pop_block_extended ) override;

    virtual std::optional<new_last_irreversible_block_t> find_new_last_irreversible_block(
      const std::vector<const witness_object*>& scheduled_witness_objects,
      const std::map<account_name_type, block_id_type>& last_fast_approved_block_by_witness,
      const unsigned witnesses_required_for_irreversiblity,
      const uint32_t old_last_irreversible ) const override;

    void on_reindex_start();
    void on_reindex_end( const std::shared_ptr<full_block_type>& end_block );
    void open(  const fc::path& file, bool enable_compression,
                int compression_level, bool enable_block_log_auto_fixing );
    void close();
    const block_log& get_block_log() const { return _block_log; }

  private:
  protected:  block_log             _block_log; private:
    //fork_db_block_reader  _reader;
  protected:   fork_database         _fork_db; private:
  
    bool                  _is_at_live_sync = false;
    database&             _db; /// Needed only for notification purposes.
    application&          _app; /// Needed only for notification purposes.
  };

} }

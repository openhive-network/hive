#pragma once
#include <hive/chain/full_block.hpp>

#include <fc/time.hpp>
#include <fc/thread/future.hpp>

#include <boost/thread/future.hpp>

namespace hive { namespace chain {

/**
 * Wrapper for full_block to control block processing (and also collect statistics).
 * Covers differences in handling when block is new, coming from p2p/API or from block log.
 */
class block_flow_control
{
public:
  enum class phase
  {
    REQUEST, //request was just created - default
    START, //request was picked from worker queue
    FORK_DB, //block was put into fork database
    FORK_APPLY, //block happened to require switching of forks - before switch (work time will be longer)
    FORK_IGNORE, //block happened to be on shorter fork - block ignored (work time will be shorter)
    FORK_NORMAL, //block happened to be on active fork - before apply (normal work time)
    APPLIED, //block was applied, but before pending reapplication
    END //finished processing
  };

  virtual ~block_flow_control() = default;

  // when block related request is picked by worker thread
  void on_worker_queue_pop() const
  {
    current_phase = phase::START;
  }

  // right after fork_db gets a block - end of work for new block
  void on_fork_db_insert() const
  {
    current_phase = phase::FORK_DB;
  }
  // right before switching fork
  void on_fork_apply() const
  {
    current_phase = phase::FORK_APPLY;
    was_fork = true;
  }
  // when block is from inactive fork
  void on_fork_ignore() const
  {
    current_phase = phase::FORK_IGNORE;
    was_ignored = true;
  }
  // when block was continuation of active fork
  void on_fork_normal() const
  {
    current_phase = phase::FORK_NORMAL;
  }
  // right before reapplication of pending transactions - end of work for block under verification
  void on_end_of_apply_block() const
  {
    current_phase = phase::APPLIED;
  }

  // after reapplication of pending transactions
  void on_end_of_processing() const
  {
    if( !except )
      current_phase = phase::END;
  }

  // in case of exception
  virtual void on_failure( const fc::exception& e ) const = 0; //call at the start of implementation in subclass
  // at the end of processing in worker thread (called even if there was exception earlier)
  virtual void on_worker_done() const;

  const std::shared_ptr<full_block_type>& get_full_block() const { return full_block; }

  phase get_phase() const { return current_phase; }
  bool finished() const { return current_phase == phase::END; }
  bool forked() const { return was_fork; }
  bool ignored() const { return was_ignored; }
  const fc::exception_ptr& get_exception() const { return except; }
  void rethrow_if_exception() const { if( except ) except->dynamic_rethrow_exception(); }

protected:
  block_flow_control( const std::shared_ptr<full_block_type>& _block )
    : full_block( _block ) {}

  // "type" of buffer to put in log/notification report (in case of success)
  virtual const char* buffer_type() const = 0;
  std::shared_ptr<full_block_type> full_block; //block being worked on

  mutable phase current_phase = phase::REQUEST; //tracks flow of work with block
  mutable bool was_fork = false; //fork was switched
  mutable bool was_ignored = false; //block was not applied because it was on shorter fork

  mutable fc::exception_ptr except; //filled in case of failure
};

/**
 * Block flow control for new block. Necessary for block producer.
 * Allows adding block after creation of this wrapper.
 */
class new_block_flow_control : public block_flow_control
{
public:
  new_block_flow_control( const fc::time_point_sec _block_ts, const protocol::account_name_type& _wo,
    const fc::ecc::private_key& _key, uint32_t _skip )
  : block_flow_control( nullptr ), block_ts( _block_ts ), witness_owner( _wo ),
    block_signing_private_key( _key ), skip( _skip ) {}
  virtual ~new_block_flow_control() = default;

  void attach_promise( const std::shared_ptr<boost::promise<void>>& _p ) { prom = _p; }
  void store_produced_block( const std::shared_ptr<full_block_type>& _block ) { full_block = _block; }

  const fc::time_point_sec& get_block_timestamp() const { return block_ts; }
  const protocol::account_name_type& get_witness_owner() const { return witness_owner; }
  const fc::ecc::private_key& get_block_signing_private_key() const { return block_signing_private_key; }
  uint32_t get_skip_flags() const { return skip; }

  virtual void on_failure( const fc::exception& e ) const override final;
  virtual void on_worker_done() const override
  {
    block_flow_control::on_worker_done();
    trigger_promise();
  }

protected:
  virtual const char* buffer_type() const override final { return "new"; }

  void trigger_promise() const
  {
    if( prom )
      prom->set_value();
  }

  fc::time_point_sec block_ts;
  protocol::account_name_type witness_owner;
  fc::ecc::private_key block_signing_private_key;
  uint32_t skip;
  std::shared_ptr<boost::promise<void>> prom;
};

/**
 * Block flow control for block coming from p2p.
 */
class p2p_block_flow_control : public block_flow_control
{
public:
  p2p_block_flow_control( const std::shared_ptr<full_block_type>& _block, uint32_t _skip )
    : block_flow_control( _block ), skip( _skip ) {}
  virtual ~p2p_block_flow_control() = default;

  void attach_promise( const fc::promise<void>::ptr& _p ) { prom = _p; }

  uint32_t get_skip_flags() const { return skip; }

  virtual void on_failure( const fc::exception& e ) const override final;
  virtual void on_worker_done() const override
  {
    block_flow_control::on_worker_done();
    trigger_promise();
  }

private:
  virtual const char* buffer_type() const override final { return "p2p"; }

  void trigger_promise() const
  {
    if( prom )
      prom->set_value();
  }

  uint32_t skip;
  fc::promise<void>::ptr prom;
};

/**
 * Block flow control for cases when no special control is needed, f.e. during tests.
 */
class existing_block_flow_control : public block_flow_control
{
public:
  existing_block_flow_control( const std::shared_ptr<full_block_type>& _block )
    : block_flow_control( _block ) {}
  virtual ~existing_block_flow_control() = default;

  virtual void on_failure( const fc::exception& e ) const override final;

protected:
  virtual const char* buffer_type() const override final { return "old"; }
};
} }

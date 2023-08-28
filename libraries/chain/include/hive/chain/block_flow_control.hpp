#pragma once
#include <hive/chain/full_block.hpp>

#include <fc/time.hpp>
#include <fc/thread/future.hpp>

#include <boost/thread/future.hpp>

namespace hive { namespace chain {

/**
 * Basic statistics for block handling.
 */
class block_stats
{
public:
  block_stats() : creation( fc::time_point::now() ) {}

  void on_start_work( uint32_t _inc_txs, uint32_t _ok_txs, uint32_t _fail_auth, uint32_t _fail_no_rc )
  {
    start_work = fc::time_point::now();
    total_inc_txs = _inc_txs;
    accepted_inc_txs = _ok_txs;
    failed_auth_txs = _fail_auth;
    failed_no_rc_txs = _fail_no_rc;
  }

  void on_end_work()
  {
    end_work = fc::time_point::now();
  }

  void on_cleanup( uint32_t _exp_txs, uint32_t _fail_txs, uint32_t _ok_txs, uint32_t _post_txs, uint32_t _lib )
  {
    end_cleanup = fc::time_point::now();
    txs_expired = _exp_txs;
    txs_failed = _fail_txs;
    txs_reapplied = _ok_txs;
    txs_postponed = _post_txs;
    last_irreversible_block_num = _lib;
  }

  fc::microseconds get_wait_time() const { return start_work - creation; }
  fc::microseconds get_work_time() const { return end_work - start_work; }
  fc::microseconds get_cleanup_time() const { return end_cleanup - end_work; }
  fc::microseconds get_total_time() const { return end_cleanup - creation; }

  const fc::time_point& get_creation_ts() const { return creation; }
  const fc::time_point& get_start_ts() const { return start_work; }
  const fc::time_point& get_ready_ts() const { return end_work; }
  const fc::time_point& get_end_ts() const { return end_cleanup; }

  uint32_t get_txs_processed_before_block() const { return total_inc_txs; }
  uint32_t get_txs_accepted_before_block() const { return accepted_inc_txs; }
  uint32_t get_txs_with_failed_auth_before_block() const { return failed_auth_txs; }
  uint32_t get_txs_with_no_rc_before_block() const { return failed_no_rc_txs; }
  uint32_t get_txs_expired_after_block() const { return txs_expired; }
  uint32_t get_txs_failed_after_block() const { return txs_failed; }
  uint32_t get_txs_reapplied_after_block() const { return txs_reapplied; }
  uint32_t get_txs_postponed_after_block() const { return txs_postponed; }

  uint32_t get_last_irreversible_block_num() const { return last_irreversible_block_num; }

private:
  //time of arrival for P2P, time of request for new block
  fc::time_point creation;
  //start of block building or verification
  fc::time_point start_work;
  //end of work on block itself, without reapplication of pending transactions and other cleanup tasks
  fc::time_point end_work;
  //end of all work related to block
  fc::time_point end_cleanup;

  //number of transactions processed before block (coming from API or P2P)
  uint32_t total_inc_txs = 0;
  //number of new transactions accepted to pending before block (coming from API or P2P)
  uint32_t accepted_inc_txs = 0;
  //number of new transactions that failed due to authorization before block (most likely old signature used)
  uint32_t failed_auth_txs = 0;
  //number of new transactions that failed due to lack of RC before block
  uint32_t failed_no_rc_txs = 0;
  //number of expired transactions during pending reapplication
  uint32_t txs_expired = 0;
  //number of transactions that failed with exception (known transactions not included) during pending reapplication
  uint32_t txs_failed = 0;
  //number of transactions successfully reapplied during pending reapplication
  uint32_t txs_reapplied = 0;
  //number of transactions that were not touched during pending reapplication due to time limit
  uint32_t txs_postponed = 0;
  //last irreversible block at the time block work was done
  uint32_t last_irreversible_block_num = 0;
};

/**
 * Wrapper for full_block to control block processing (and also collect statistics).
 * Covers differences in handling when block is new, coming from p2p/API or from block log.
 */
class block_flow_control
{
public:
  enum class report_type
  {
    NONE, //no report
    MINIMAL, //just basic block stats
    REGULAR, //almost everything, useful for non-API nodes
    FULL //everything, useful for API nodes
  };
  enum class report_output { DLOG, ILOG, NOTIFY };

  static void set_auto_report( const std::string& _option_type, const std::string& _option_output );
  static void set_auto_report( report_type _type, report_output _output )
  {
    auto_report_type = _type;
    auto_report_output = _output;
  }

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

  // when block related request is picked by worker thread (only overridden in tests)
  virtual void on_write_queue_pop( uint32_t _inc_txs, uint32_t _ok_txs, uint32_t _fail_auth, uint32_t _fail_no_rc ) const;

  // right after fork_db gets a block - end of work for new block
  virtual void on_fork_db_insert() const;
  // right before switching fork (only overridden in tests)
  virtual void on_fork_apply() const;
  // when block is from inactive fork (only overridden in tests)
  virtual void on_fork_ignore() const;
  // when block was continuation of active fork (only overridden in tests)
  virtual void on_fork_normal() const;
  // right before reapplication of pending transactions - end of work for block under verification
  virtual void on_end_of_apply_block() const;

  // after reapplication of pending transactions (only overridden in tests)
  virtual void on_end_of_processing( uint32_t _exp_txs, uint32_t _fail_txs, uint32_t _ok_txs, uint32_t _post_txs, uint32_t _lib ) const;

  // in case of exception
  virtual void on_failure( const fc::exception& e ) const;
  // at the end of processing in worker thread (called even if there was exception earlier)
  virtual void on_worker_done() const;

  virtual fc::time_point_sec get_block_timestamp() const = 0;
  const std::shared_ptr<full_block_type>& get_full_block() const { return full_block; }

  const block_stats& get_stats() const { return stats; }
  fc::variant_object get_report( report_type rt ) const;

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

  mutable block_stats stats;
  mutable phase current_phase = phase::REQUEST; //tracks flow of work with block
  mutable bool was_fork = false; //fork was switched
  mutable bool was_ignored = false; //block was not applied because it was on shorter fork

  mutable fc::exception_ptr except; //filled in case of failure

  static report_type auto_report_type; //type of automatic block stats reports (disabled for sync blocks)
  static report_output auto_report_output; //output of automatic block stat reports
};

/**
 * Block flow control for newly generated block. Necessary for block producer.
 * Allows adding block after creation of this wrapper.
 */
class generate_block_flow_control : public block_flow_control
{
public:
  generate_block_flow_control( const fc::time_point_sec _block_ts, const protocol::account_name_type& _wo,
    const fc::ecc::private_key& _key, uint32_t _skip )
  : block_flow_control( nullptr ), block_ts( _block_ts ), witness_owner( _wo ),
    block_signing_private_key( _key ), skip( _skip ) {}
  virtual ~generate_block_flow_control() = default;

  void attach_promise( const std::shared_ptr<boost::promise<void>>& _p ) { prom = _p; }
  void store_produced_block( const std::shared_ptr<full_block_type>& _block ) { full_block = _block; }

  const protocol::account_name_type& get_witness_owner() const { return witness_owner; }
  const fc::ecc::private_key& get_block_signing_private_key() const { return block_signing_private_key; }
  uint32_t get_skip_flags() const { return skip; }

  virtual void on_fork_db_insert() const override; //supplemented with broadcast in witness_plugin
  virtual void on_end_of_apply_block() const override;
  virtual void on_failure( const fc::exception& e ) const override;

  virtual fc::time_point_sec get_block_timestamp() const override final { return block_ts; }

protected:
  virtual const char* buffer_type() const override final { return "gen"; }

  inline void trigger_promise() const;

  fc::time_point_sec block_ts;
  protocol::account_name_type witness_owner;
  fc::ecc::private_key block_signing_private_key;
  uint32_t skip;
  mutable std::shared_ptr<boost::promise<void>> prom;
};

/**
 * Block flow control for block coming from p2p.
 */
class p2p_block_flow_control : public block_flow_control
{
public:
  p2p_block_flow_control( const std::shared_ptr<full_block_type>& _block, uint32_t _skip )
    : block_flow_control( _block ), skip( _skip ) { FC_ASSERT( _block ); }
  virtual ~p2p_block_flow_control() = default;

  void attach_promise( const fc::promise<void>::ptr& _p ) { prom = _p; }

  uint32_t get_skip_flags() const { return skip; }

  virtual void on_end_of_apply_block() const override;
  virtual void on_failure( const fc::exception& e ) const override;

  virtual fc::time_point_sec get_block_timestamp() const override final;

private:
  virtual const char* buffer_type() const override { return "p2p"; }

  inline void trigger_promise() const;

  uint32_t skip;
  mutable fc::promise<void>::ptr prom;
};

/**
 * Block flow control for block coming from sync mode p2p.
 */
class sync_block_flow_control : public p2p_block_flow_control
{
public:
  using p2p_block_flow_control::p2p_block_flow_control;
  virtual ~sync_block_flow_control() = default;

  virtual void on_worker_done() const override;

private:
  virtual const char* buffer_type() const override final { return "sync"; }
};

/**
 * Block flow control for cases when no special control is needed, f.e. during tests.
 */
class existing_block_flow_control : public block_flow_control
{
public:
  existing_block_flow_control( const std::shared_ptr<full_block_type>& _block )
    : block_flow_control( _block ) { FC_ASSERT( _block ); }
  virtual ~existing_block_flow_control() = default;

  virtual void on_end_of_apply_block() const override;

  virtual fc::time_point_sec get_block_timestamp() const override final;

protected:
  virtual const char* buffer_type() const override final { return "old"; }
};

} }

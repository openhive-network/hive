#pragma once
#include <hive/protocol/block.hpp>

#include <fc/time.hpp>
#include <fc/thread/future.hpp>

#include <boost/thread/future.hpp>
#include <boost/shared_ptr.hpp>

namespace hive { namespace chain {

/**
 * Basic statistics for block handling.
 */
class block_stats
{
public:
  block_stats() : creation( fc::time_point::now() ) {}

  void on_start_work( uint32_t _inc_txs, uint32_t _ok_txs )
  {
    start_work = fc::time_point::now();
    total_inc_txs = _inc_txs;
    accepted_inc_txs = _ok_txs;
  }

  void on_end_work()
  {
    end_work = fc::time_point::now();
  }

  void on_cleanup( uint32_t _exp_txs, uint32_t _fail_txs, uint32_t _ok_txs, uint32_t _post_txs )
  {
    end_cleanup = fc::time_point::now();
    txs_expired = _exp_txs;
    txs_failed = _fail_txs;
    txs_reapplied = _ok_txs;
    txs_postponed = _post_txs;
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
  uint32_t get_txs_expired_after_block() const { return txs_expired; }
  uint32_t get_txs_failed_after_block() const { return txs_failed; }
  uint32_t get_txs_reapplied_after_block() const { return txs_reapplied; }
  uint32_t get_txs_postponed_after_block() const { return txs_postponed; }

private:
  //time of arrival for P2P, time of request for new block, time of fetch for block_log
  fc::time_point creation;
  //start of block building, verification or reapplication
  fc::time_point start_work;
  //end of work on block itself, without reapplication of pending transactions and other cleanup tasks
  fc::time_point end_work;
  //end of all work related to block
  fc::time_point end_cleanup;

  //number of transactions processed before block (coming from API or P2P)
  uint32_t total_inc_txs = 0;
  //number of new transactions accepted to pending before block (coming from API or P2P)
  uint32_t accepted_inc_txs = 0;
  //number of expired transactions during pending reapplication
  uint32_t txs_expired = 0;
  //number of transactions that failed with exception (known transactions not included) during pending reapplication
  uint32_t txs_failed = 0;
  //number of transactions successfully reapplied during pending reapplication
  uint32_t txs_reapplied = 0;
  //number of transactions that were not touched during pending reapplication due to time limit
  uint32_t txs_postponed = 0;
};

/**
 * Internal wrapper for signed_block.
 * Used to carry extra statistical/debug and optimization data and cover differences
 * in handling when block is new, coming from p2p/API or from block log.
 */
class block_data
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

  virtual ~block_data() = default;

  // when block related request is picked by worker thread
  void on_worker_queue_pop( uint32_t _inc_txs = 0, uint32_t _ok_txs = 0 )
  {
    stats.on_start_work( _inc_txs, _ok_txs );
    current_phase = phase::START;
  }

  // right after fork_db gets a block - end of work for new block
  virtual void on_fork_db_insert() = 0;
  // right before switching fork
  void on_fork_apply()
  {
    current_phase = phase::FORK_APPLY;
    was_fork = true;
  }
  // when block is from inactive fork
  void on_fork_ignore()
  {
    current_phase = phase::FORK_IGNORE;
    was_ignored = true;
  }
  // when block was continuation of active fork
  void on_fork_normal()
  {
    current_phase = phase::FORK_NORMAL;
  }
  // right before reapplication of pending transactions - end of work for block under verification
  virtual void on_end_of_apply_block() = 0;

  // after reapplication of pending transactions
  void on_end_of_processing( uint32_t _exp_txs, uint32_t _fail_txs, uint32_t _ok_txs, uint32_t _post_txs )
  {
    stats.on_cleanup( _exp_txs, _fail_txs, _ok_txs, _post_txs );
    if( !except )
      current_phase = phase::END;
  }

  // in case of exception
  virtual void on_failure( const fc::exception& e ) = 0;
  // at the end of processing in worker thread (called even if there was exception earlier)
  virtual void on_worker_done();

  const protocol::signed_block& get_block() const { return *block; }
  boost::shared_ptr< protocol::signed_block > get_block_ptr() const { return block; }
  size_t get_block_size() const { return block_size; } //not necessary once we have everything within block

  const block_stats& get_stats() const { return stats; }
  void log_stats() const;
  void notify_stats() const;

  phase get_phase() const { return current_phase; }
  bool finished() const { return current_phase == phase::END; }
  bool forked() const { return was_fork; }
  bool ignored() const { return was_ignored; }
  fc::optional< fc::exception > get_exception() const { return except; }
  void rethrow_if_exception() const { if( except ) throw *except; }

protected:
  block_data( boost::shared_ptr< protocol::signed_block >&& _block, size_t _size )
    : block( std::move( _block ) ), block_size( _size ){}

  // "type" of buffer to put in log/notification report (in case of success)
  virtual const char* buffer_type() const = 0;

  // TODO: boost version used only because it is used in fork_db
  // NOTE: what is currently just signed_block, in final version should contain binary form as well
  // as space for block invariants, and it should be the same data that is stored inside fork_db
  boost::shared_ptr< protocol::signed_block > block;
  // TODO: we won't need size separately once block contains its packed version
  size_t block_size = 0;

  block_stats stats;
  phase current_phase = phase::REQUEST;
  bool was_fork = false; //fork was switched
  bool was_ignored = false; //block was not applied because it was on shorter fork

  fc::optional< fc::exception > except;
};

/**
 * Block data version for new block. Necessary for block producer.
 * Allows adding block after wrapper is created.
 * Triggers given promise right after insertion of new block into fork database.
 * Replaces former generate_block_request.
 */
class new_block_data : public block_data
{
public:
  new_block_data( const fc::time_point_sec _block_ts, const protocol::account_name_type& _wo,
    const fc::ecc::private_key& _key, uint32_t _skip )
  : block_data( boost::make_shared< protocol::signed_block >(), 0 ),
    block_ts( _block_ts ), witness_owner( _wo ), block_signing_private_key( _key ), skip( _skip )
  {
    ilog( "Requesting new block for ts: ${t}", ( "t", _block_ts ) );
  }
  virtual ~new_block_data() = default;

  void attach_promise( std::shared_ptr<boost::promise<void>> _p )
  {
    prom = std::move( _p );
  }

  protocol::signed_block* get_block_to_fill() { return block.get(); }
  void on_finished_block( size_t _size ) { block_size = _size; } //won't be necessary in final version, since size will be inside block itself

  const fc::time_point_sec& get_block_timestamp() const { return block_ts; }
  const protocol::account_name_type& get_witness_owner() const { return witness_owner; }
  const fc::ecc::private_key& get_block_signing_private_key() const { return block_signing_private_key; }
  uint32_t get_skip_flags() const { return skip; }

  virtual void on_fork_db_insert() override; //to be supplemented with broadcast in witness_plugin
  virtual void on_end_of_apply_block() override final;
  virtual void on_failure( const fc::exception& e ) override final;

private:
  virtual const char* buffer_type() const { return "new"; }

  fc::time_point_sec block_ts;
  protocol::account_name_type witness_owner;
  fc::ecc::private_key block_signing_private_key;
  uint32_t skip;
  std::shared_ptr<boost::promise<void>> prom;
};

/**
 * Block data version for block coming from p2p.
 * Triggers given promise after block is applied but before reapplication of pending transactions.
 */
class inc_block_data final : public block_data
{
public:
  inc_block_data( boost::shared_ptr< protocol::signed_block > _block, size_t _size, uint32_t _skip )
    : block_data( std::move( _block ), _size ), skip( _skip ) {}
  virtual ~inc_block_data() = default;

  void attach_promise( fc::promise<void>::ptr _p )
  {
    prom = std::move( _p );
  }

  uint32_t get_skip_flags() const { return skip; }

  virtual void on_fork_db_insert() override;
  virtual void on_end_of_apply_block() override;
  virtual void on_failure( const fc::exception& e ) override;

private:
  virtual const char* buffer_type() const { return "p2p"; }

  uint32_t skip;
  fc::promise<void>::ptr prom;
};

/**
 * Block data for block that was previously applied.
 * Useful f.e. for reapplying popped blocks.
 * Can also be used when no special control is needed, in particular, no thread is waiting
 * for promise to be filled.
 */
class old_block_data final : public block_data
{
public:
  old_block_data( boost::shared_ptr< protocol::signed_block > _block, size_t _size )
    : block_data( std::move( _block ), _size ) {}
  virtual ~old_block_data() = default;

  virtual void on_fork_db_insert() override;
  virtual void on_end_of_apply_block() override;
  virtual void on_failure( const fc::exception& e ) override;

private:
  virtual const char* buffer_type() const { return "old"; }
};

} }

#pragma once
#include <memory>
#include <hive/chain/full_block.hpp>
#include <hive/chain/full_transaction.hpp>

namespace appbase {
  class application;
}

namespace hive { namespace chain {

// thread pool for offloading as much work as possible on blocks and transactions.
// right now, this mostly applies to blocks & transactions coming from the p2p 
// network.  These threads will handle compressing/decompressing the blocks,
// deserializing blocks and transactions, validating transaction invariants,
// and doing some of the expensive crypto operations used for signature 
// validation.
// 
// All the knowledge of what needs to be pre-computed is built in to this
// thread pool.  If you want to extend it to do other work, you'll need to
// modify the code that processes the queue.
//
// It's possible that the user will configure their hived to use zero 
// threads.  In that case, calls to enqueue_work() will do nothing.  The
// full_block and full_transaction classes are designed so that if you call,
// say, full_transaction::validate() and the this pool has zero workers or otherwise
// hasn't gotten around to pre-validating it, the thread that calls validate()
// will perform the work itself.
class blockchain_worker_thread_pool
{
public:
  struct impl;
private:
  class impl_deleter
  {
  public:
    void operator()(blockchain_worker_thread_pool::impl* ptr) const;
  };

  std::unique_ptr<impl, impl_deleter> my;
public:

  blockchain_worker_thread_pool( appbase::application& app );
  ~blockchain_worker_thread_pool();

  // when we process a block/transaction, we need to know where it came from in
  // order to know what processing it needs.  e.g., transactions that arrived
  // in blocks usually won't have their signatures validated, while stand-alone
  // transactions will.
  enum class data_source_type
  {
    block_received_from_p2p,
    transaction_inside_block_received_from_p2p,
    transaction_inside_block_for_replay,
    standalone_transaction_received_from_p2p,
    standalone_transaction_received_from_api,
    locally_produced_block,
    block_log_destined_for_p2p_compressed,
    block_log_destined_for_p2p_uncompressed,
    block_log_destined_for_p2p_alternate_compressed,
    block_log_for_replay,
    block_log_for_decompressing,
    block_log_for_artifact_generation
  };
  void enqueue_work(const std::shared_ptr<full_block_type>& full_block, data_source_type data_source);
  void enqueue_work(const std::shared_ptr<full_transaction_type>& full_transaction, data_source_type data_source);
  void enqueue_work(const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions, data_source_type data_source,
                    std::optional<uint32_t> block_number);

  void set_p2p_force_validate();
  void set_validate_during_replay();
  void set_is_block_producer();
  void set_last_checkpoint(uint32_t last_checkpoint);

  void shutdown();
  void set_thread_pool_size(uint32_t thread_pool_size);
};

} } // end namespace hive::chain


#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <appbase/application.hpp>

#include <fc/thread/thread.hpp>

#include <boost/lockfree/queue.hpp>

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <variant>
#include <vector>
#include <array>

namespace hive { namespace chain {

namespace
{
  uint32_t thread_pool_size = 0;
}

struct blockchain_worker_thread_pool::impl
{
  struct work_request_type
  {
    std::variant<std::weak_ptr<full_block_type>, std::weak_ptr<full_transaction_type>> block_or_transaction;
    blockchain_worker_thread_pool::data_source_type data_source;
  };
  typedef boost::lockfree::queue<work_request_type*> queue_type;
  // we separate jobs into high, medium, and low priority.  worker threads will empty the high-priority
  // queue before taking anything from the medium-priority queue, and empty the medium-priority queue
  // before working on low-priority jobs.
  enum class priority_type { high, medium, low };
  std::array<queue_type, 3> work_queues{queue_type{1000}, queue_type{1000}, queue_type{1000}};
  std::atomic<unsigned> number_of_items_in_queue;
  std::mutex work_queue_mutex;
  std::condition_variable work_queue_condition_variable;
  std::atomic<bool> running = { true };
  
  std::vector<std::thread> threads;

  bool p2p_force_validate = false;
  bool validate_during_replay = false;

  bool allow_enqueue_work() const;
  bool is_running() const;

  bool dequeue_work(work_request_type*& work_request_ptr);

  void perform_work(const std::weak_ptr<full_block_type>& full_block, data_source_type data_source);
  void perform_work(const std::weak_ptr<full_transaction_type>& full_transaction, data_source_type data_source);
  void thread_function();
};

bool blockchain_worker_thread_pool::impl::allow_enqueue_work() const
{
  return !threads.empty() && is_running();
}

bool blockchain_worker_thread_pool::impl::is_running() const
{
  return running.load(std::memory_order_relaxed) && !appbase::app().is_interrupt_request();
}

bool blockchain_worker_thread_pool::impl::dequeue_work(work_request_type*& work_request_ptr)
{
  return std::find_if(work_queues.begin(), work_queues.end(), 
                      [&](queue_type& queue) { return queue.pop(work_request_ptr); }) != work_queues.end();
}

void blockchain_worker_thread_pool::impl::thread_function()
{
  work_request_type* work_request_raw_ptr = nullptr;
  while (true)
  {
    if (!is_running())
      return;
    if (!dequeue_work(work_request_raw_ptr))
    {
      // queue was empty, wait for work
      std::unique_lock<std::mutex> lock(work_queue_mutex);
      while (is_running() && !dequeue_work(work_request_raw_ptr))
        work_queue_condition_variable.wait(lock);
      if (!is_running())
        return;
    }

    --number_of_items_in_queue;

    // we have work, take ownership of the pointer
    std::unique_ptr<work_request_type> work_request(work_request_raw_ptr);

    // perform work on work_request
    std::visit([&](const auto& block_or_transaction) { perform_work(block_or_transaction, work_request->data_source); }, 
               work_request->block_or_transaction);
  }
}

//std::shared_ptr<std::thread> fill_queue_thread = std::make_shared<std::thread>([&](){ fill_pending_queue(input_block_log_path / "block_log"); });
blockchain_worker_thread_pool::blockchain_worker_thread_pool() :
  my(std::make_unique<impl>())
{
  for (unsigned i = 1; i <= thread_pool_size; ++i)
  {
    my->threads.emplace_back([i, this]() {
      std::ostringstream thread_name_stream;
      thread_name_stream << "worker_" << i << "_of_" << thread_pool_size;
      std::string thread_name = thread_name_stream.str();
      fc::set_thread_name(thread_name.c_str()); // tells the OS the thread's name
      fc::thread::current().set_name(thread_name); // tells fc the thread's name for logging
      my->thread_function();
    });
  }
}

void blockchain_worker_thread_pool::impl::perform_work(const std::weak_ptr<full_block_type>& full_block_weak_ptr, data_source_type data_source)
{
  try
  {
    std::shared_ptr<full_block_type> full_block = full_block_weak_ptr.lock();
    if (!full_block)
      return; // the block was garbage collected before we could do any work on it
    // fc::time_point start = fc::time_point::now();
    switch (data_source)
    {
      case blockchain_worker_thread_pool::data_source_type::block_received_from_p2p:
        // fully decompress (if necessary) the block and unpack it
        full_block->decode_block();

        // now we have the full_transactions, get started working on them
        blockchain_worker_thread_pool::get_instance().enqueue_work(full_block->get_full_transactions(), 
                                                                   blockchain_worker_thread_pool::data_source_type::transaction_inside_block_received_from_p2p);
        // precompute some stuff we'll need for validating the block
        full_block->compute_signing_key();
        full_block->compute_merkle_root();

        // compute the legacy block message hash for sharing on the network
        // TODO: remove this after the hardfork
        full_block->compute_legacy_block_message_hash();

        // finally, compress it if it didn't start out that way (needed for writing to the block log)
        full_block->compress_block();
        break;
      case blockchain_worker_thread_pool::data_source_type::locally_produced_block:
        // locally-produced blocks should have everything done except for the compression, so kick that off now
        full_block->compress_block();

        // compute the legacy block message hash for sharing on the network
        // TODO: remove this after the hardfork
        full_block->compute_legacy_block_message_hash();

        break;
      case blockchain_worker_thread_pool::data_source_type::block_log_destined_for_p2p_compressed:
        // if we're reading from an uncompressed block, but sending to a peer that wants compressed blocks, 
        // compress it for them
        full_block->compress_block();
        break;
      case blockchain_worker_thread_pool::data_source_type::block_log_destined_for_p2p_uncompressed:
        // if we're reading a compressed block log serving blocks to a peer who can't accept 
        // them, decompress the block
        // TODO: remove this after the hardfork
        full_block->decompress_block();
        break;
      case blockchain_worker_thread_pool::data_source_type::block_log_destined_for_p2p_alternate_compressed:
        // if we're reading a compressed block log serving blocks to a peer who can accept compressed blocks,
        // but ours is compressed using a dictionary they don't have, recompress our block using no dictionary
        full_block->alternate_compress_block();
        break;
      case blockchain_worker_thread_pool::data_source_type::block_log_for_artifact_generation:
        full_block->decode_block_header();
        break;
      case blockchain_worker_thread_pool::data_source_type::block_log_for_replay:
        // fully decompress (if necessary) the block and unpack it
        full_block->decode_block();

        // precompute some stuff we'll need for validating the block
        if (validate_during_replay)
        {
          // now we have the full_transactions, get started working on them
          blockchain_worker_thread_pool::get_instance().enqueue_work(full_block->get_full_transactions(), 
                                                                     blockchain_worker_thread_pool::data_source_type::transaction_inside_block_for_replay);
          full_block->compute_signing_key();
          full_block->compute_merkle_root();
        }
      case blockchain_worker_thread_pool::data_source_type::block_log_for_decompressing:
        full_block->decompress_block();
        break;
      default:
        elog("Error: full block added to worker thread with an unrecognized data source");
    }
  }
  catch (const fc::exception& e)
  {
    // note: none of the above calls is expected to throw during normal operation.  But if something unexpected
    // happens, we don't want it to kill the worker threads
    elog("caught unexpected exception: ${e}", (e));
  }
  catch (const std::exception& e)
  {
    elog("caught unexpected exception: ${what}", ("what", e.what()));
    edump((e.what()));
  }
  catch (...)
  {
    elog("caught unexpected exception");
  }
  // fc::time_point end = fc::time_point::now();
  // fc::microseconds duration = end - start;
  // ilog("perform_work on the block took ${duration}μs", (duration));
}

void blockchain_worker_thread_pool::impl::perform_work(const std::weak_ptr<full_transaction_type>& full_transaction_weak_ptr, data_source_type data_source)
{
  std::shared_ptr<full_transaction_type> full_transaction = full_transaction_weak_ptr.lock();
  if (!full_transaction)
    return; // the transaction was garbage collected before we could do any work on it

  switch (data_source)
  {
    case blockchain_worker_thread_pool::data_source_type::transaction_inside_block_received_from_p2p:
      // this depends a bit on what the current validation settings are
      try
      {
        // validate is always called during normal block processing
        full_transaction->validate();
      }
      catch (...)
      {
        // we ignore exceptions for all calls, we're just trying to make the full_transaction precompute the
        // result (or, if there's an error, precompute the exception).  Just like with a normal result, the
        // full_transaction will cache any exceptions thrown now and rethrow them when and if the blockchain
        // makes the corresponding call during the course of apply_transaction.
      }

      // but by default, signature validation isn't done unless you specify --p2p-force-validate
      if (p2p_force_validate)
      {
        try
        {
          (void)full_transaction->get_signature_keys();
        }
        catch (...)
        {
        }

        try
        {
          (void)full_transaction->get_required_authorities();
        }
        catch (...)
        {
        }
      }
      break;
    case blockchain_worker_thread_pool::data_source_type::transaction_inside_block_for_replay:
      // by default very little checking is done during replay, unless you specify --validate_during_replay
      // NOTE: currently, transactions aren't enqueued unless validate_during_replay is true, so this
      // check is redundant.  But if you do add some work that needs to happen when validate_during_replay
      // is false, you'll need to change the block's perform_work to enqueue transactions
      if (validate_during_replay)
      {
        try
        {
          // validate is always called during reindex and normal block processing
          full_transaction->validate();
        }
        catch (...)
        {
          // we ignore exceptions for all calls, we're just trying to make the full_transaction precompute the
          // result (or, if there's an error, precompute the exception).  Just like with a normal result, the
          // full_transaction will cache any exceptions thrown now and rethrow them when and if the blockchain
          // makes the corresponding call during the course of apply_transaction.
        }

        try
        {
          (void)full_transaction->get_signature_keys();
        }
        catch (...)
        {
        }

        try
        {
          (void)full_transaction->get_required_authorities();
        }
        catch (...)
        {
        }
      }
      break;
    case blockchain_worker_thread_pool::data_source_type::standalone_transaction_received_from_p2p:
    case blockchain_worker_thread_pool::data_source_type::standalone_transaction_received_from_api:
      // check this, but I think all standalone transactions will need full validation
      try
      {
        full_transaction->validate();
      }
      catch (...)
      {
      }

      try
      {
        (void)full_transaction->get_signature_keys();
      }
      catch (...)
      {
      }
      try
      {
        (void)full_transaction->get_required_authorities();
      }
      catch (...)
      {
      }
      break;
    default:
      elog("invalid data source type for transaction");
  }
}

namespace
{
  blockchain_worker_thread_pool::impl::priority_type get_priority_for_block(blockchain_worker_thread_pool::data_source_type data_source)
  {
    switch (data_source)
    {
      case blockchain_worker_thread_pool::data_source_type::block_received_from_p2p:
        return blockchain_worker_thread_pool::impl::priority_type::medium;
      case blockchain_worker_thread_pool::data_source_type::locally_produced_block:
        return blockchain_worker_thread_pool::impl::priority_type::high;
      case blockchain_worker_thread_pool::data_source_type::block_log_destined_for_p2p_compressed:
      case blockchain_worker_thread_pool::data_source_type::block_log_destined_for_p2p_uncompressed:
      case blockchain_worker_thread_pool::data_source_type::block_log_destined_for_p2p_alternate_compressed:
        return blockchain_worker_thread_pool::impl::priority_type::low;
      case blockchain_worker_thread_pool::data_source_type::block_log_for_artifact_generation:
        return blockchain_worker_thread_pool::impl::priority_type::high; // nothing else will be running, priority doesn't matter
      case blockchain_worker_thread_pool::data_source_type::block_log_for_replay:
        return blockchain_worker_thread_pool::impl::priority_type::medium;
      case blockchain_worker_thread_pool::data_source_type::block_log_for_decompressing:
        return blockchain_worker_thread_pool::impl::priority_type::high; // nothing else will be running, priority doesn't matter
      default:
        elog("invalid data source type for block");
        return blockchain_worker_thread_pool::impl::priority_type::low;
    }
  }

  blockchain_worker_thread_pool::impl::priority_type get_priority_for_transaction(blockchain_worker_thread_pool::data_source_type data_source)
  {
    switch (data_source)
    {
      case blockchain_worker_thread_pool::data_source_type::transaction_inside_block_received_from_p2p:
      case blockchain_worker_thread_pool::data_source_type::transaction_inside_block_for_replay:
        return blockchain_worker_thread_pool::impl::priority_type::high;
      case blockchain_worker_thread_pool::data_source_type::standalone_transaction_received_from_p2p:
      case blockchain_worker_thread_pool::data_source_type::standalone_transaction_received_from_api:
        return blockchain_worker_thread_pool::impl::priority_type::low;
      default:
        elog("invalid data source type for transaction");
        return blockchain_worker_thread_pool::impl::priority_type::low;
    }
  }
}

void blockchain_worker_thread_pool::enqueue_work(const std::shared_ptr<full_block_type>& full_block, data_source_type data_source)
{
  if (!my->allow_enqueue_work())
    return;
  std::unique_ptr<impl::work_request_type> work_request(new impl::work_request_type{full_block, data_source});
  impl::priority_type priority = get_priority_for_block(data_source);
  {
    std::unique_lock<std::mutex> lock(my->work_queue_mutex);
    my->work_queues[(unsigned)priority].push(work_request.release());
  }

  ++my->number_of_items_in_queue;
  //idump((my->number_of_items_in_queue.load()));
  my->work_queue_condition_variable.notify_one();
}

void blockchain_worker_thread_pool::enqueue_work(const std::shared_ptr<full_transaction_type>& full_transaction, data_source_type data_source)
{
  if (!my->allow_enqueue_work())
    return;
  std::unique_ptr<impl::work_request_type> work_request(new impl::work_request_type{full_transaction, data_source});
  impl::priority_type priority = get_priority_for_transaction(data_source);
  {
    std::unique_lock<std::mutex> lock(my->work_queue_mutex);
    my->work_queues[(unsigned)priority].push(work_request.release());
  }

  ++my->number_of_items_in_queue;
  my->work_queue_condition_variable.notify_one();
}

void blockchain_worker_thread_pool::enqueue_work(const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions, data_source_type data_source)
{
  if (!my->allow_enqueue_work())
    return;
  // build a list of work requests
  std::vector<std::unique_ptr<impl::work_request_type>> work_requests;
  work_requests.reserve(full_transactions.size());
  std::transform(full_transactions.begin(), full_transactions.end(),
                 std::back_inserter(work_requests),
                 [&](const std::shared_ptr<full_transaction_type>& full_transaction) { 
    return std::unique_ptr<impl::work_request_type>(new impl::work_request_type{full_transaction, data_source});
  });
  impl::priority_type priority = get_priority_for_transaction(data_source);

  // then enqueue them all at once
  {
    std::unique_lock<std::mutex> lock(my->work_queue_mutex);
    std::for_each(work_requests.begin(), work_requests.end(), [&](std::unique_ptr<impl::work_request_type>& work_request) { 
      my->work_queues[(unsigned)priority].push(work_request.release());
      ++my->number_of_items_in_queue;
    });
  }
  my->work_queue_condition_variable.notify_all();
}

void blockchain_worker_thread_pool::set_p2p_force_validate()
{
  my->p2p_force_validate = true;
}

void blockchain_worker_thread_pool::set_validate_during_replay()
{
  my->validate_during_replay = true;
}

void blockchain_worker_thread_pool::shutdown()
{
  ilog("shutting down worker threads");
  my->running.store(false, std::memory_order_relaxed);
  my->work_queue_condition_variable.notify_all();
  std::for_each(my->threads.begin(), my->threads.end(), [](std::thread& thread) { thread.join(); });
  ilog("worker threads successfully shut down");
}

// this only works if called before the singleton instance is created
/* static */ void blockchain_worker_thread_pool::set_thread_pool_size(uint32_t new_thread_pool_size)
{
  thread_pool_size = new_thread_pool_size;
}

/* static */ blockchain_worker_thread_pool& blockchain_worker_thread_pool::get_instance()
{
  static blockchain_worker_thread_pool thread_pool;
  return thread_pool;
}

} } // end namespace hive::chain


#pragma once
#include <hive/chain/full_block.hpp>

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace hive { namespace chain {

// Drops references to blocks that are no longer needed on a dedicated thread, so that freeing
// a block with all its transactions (which, for a full late-history block, is a noticeable
// part of applying it) does not run on the thread that applies blocks.
//
// The queue is bounded: when the releaser falls behind, release() blocks the caller until there
// is room, so references are never dropped early and memory does not grow without limit.
// Destroying the releaser (also during stack unwinding) drops every queued reference before it
// returns.
//
// Handing over a reference only moves where it is dropped; if other owners still hold the block,
// it stays alive until they let it go, as before.
class applied_block_releaser
{
public:
  static constexpr size_t default_max_queued_blocks = 256;

  explicit applied_block_releaser( size_t max_queued_blocks = default_max_queued_blocks );
  ~applied_block_releaser();

  applied_block_releaser( const applied_block_releaser& ) = delete;
  applied_block_releaser& operator=( const applied_block_releaser& ) = delete;

  // takes over the caller's reference (null is ignored); blocks while the queue is full
  void release( std::shared_ptr<full_block_type>&& block );

private:
  void run();

  const size_t _max_queued_blocks;
  std::mutex _mutex;
  std::condition_variable _work_available;
  std::condition_variable _space_available;
  std::vector<std::shared_ptr<full_block_type>> _queue;
  bool _stopping = false;
  std::thread _thread;
};

} } // hive::chain

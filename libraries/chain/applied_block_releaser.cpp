#include <hive/chain/applied_block_releaser.hpp>

#include <fc/exception/exception.hpp>
#include <fc/thread/thread.hpp>

namespace hive { namespace chain {

applied_block_releaser::applied_block_releaser( size_t max_queued_blocks )
  : _max_queued_blocks( max_queued_blocks )
{
  FC_ASSERT( _max_queued_blocks > 0 );
  _queue.reserve( _max_queued_blocks );
  _thread = std::thread( [this]() { run(); } );
}

applied_block_releaser::~applied_block_releaser()
{
  {
    std::unique_lock<std::mutex> lock( _mutex );
    _stopping = true;
  }
  _work_available.notify_one();
  _thread.join();
}

void applied_block_releaser::release( std::shared_ptr<full_block_type>&& block )
{
  if( !block )
    return;
  {
    std::unique_lock<std::mutex> lock( _mutex );
    _space_available.wait( lock, [this]() { return _queue.size() < _max_queued_blocks; } );
    _queue.push_back( std::move( block ) );
  }
  _work_available.notify_one();
}

void applied_block_releaser::run()
{
  fc::set_thread_name( "block_release" ); // tells the OS the thread's name
  fc::thread::current().set_name( "block_release" ); // tells fc the thread's name for logging

  std::vector<std::shared_ptr<full_block_type>> to_release;
  to_release.reserve( _max_queued_blocks );
  for( ;; )
  {
    bool stopping = false;
    {
      std::unique_lock<std::mutex> lock( _mutex );
      _work_available.wait( lock, [this]() { return _stopping || !_queue.empty(); } );
      to_release.swap( _queue );
      stopping = _stopping;
    }
    _space_available.notify_all();
    // the actual release, outside the lock so release() is not held up by it
    to_release.clear();
    // _stopping is only set by the destructor, after which release() is no longer called,
    // so the queue taken together with the flag was the last one
    if( stopping )
      return;
  }
}

} } // hive::chain

#include <boost/test/unit_test.hpp>

#include <hive/chain/applied_block_releaser.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

using namespace hive::chain;

namespace {

// a block whose destruction reports the thread that freed it
std::shared_ptr<full_block_type> make_tracked_block( std::mutex& mutex, std::set<std::thread::id>& freeing_threads,
  std::atomic<uint32_t>& freed_count )
{
  return std::shared_ptr<full_block_type>( new full_block_type(), [&]( full_block_type* block )
  {
    {
      std::lock_guard<std::mutex> lock( mutex );
      freeing_threads.insert( std::this_thread::get_id() );
    }
    delete block;
    freed_count.fetch_add( 1 );
  } );
}

}

BOOST_AUTO_TEST_SUITE( applied_block_releaser_tests )

BOOST_AUTO_TEST_CASE( frees_blocks_off_the_caller_thread_and_drains_on_destruction )
{
  constexpr uint32_t block_count = 1000;
  std::mutex mutex;
  std::set<std::thread::id> freeing_threads;
  std::atomic<uint32_t> freed_count = { 0 };
  std::shared_ptr<full_block_type> kept;
  {
    // a small bound, so release() has to wait for room many times
    applied_block_releaser releaser( 4 );
    for( uint32_t i = 0; i < block_count; ++i )
    {
      std::shared_ptr<full_block_type> block = make_tracked_block( mutex, freeing_threads, freed_count );
      if( i == 0 )
        kept = block;
      releaser.release( std::move( block ) );
      BOOST_REQUIRE( !block );
    }
    std::shared_ptr<full_block_type> null_block;
    releaser.release( std::move( null_block ) );
  }

  // every block handed over was freed by the time the releaser was destroyed, except the one still
  // referenced elsewhere: only the releaser's own reference was dropped
  BOOST_REQUIRE_EQUAL( freed_count.load(), block_count - 1 );
  BOOST_REQUIRE_EQUAL( kept.use_count(), 1 );
  BOOST_REQUIRE_EQUAL( freeing_threads.size(), 1u );
  BOOST_REQUIRE( *freeing_threads.begin() != std::this_thread::get_id() );

  kept.reset();
  BOOST_REQUIRE_EQUAL( freed_count.load(), block_count );
}

BOOST_AUTO_TEST_CASE( release_waits_while_the_queue_is_full )
{
  std::promise<void> first_free_started;
  std::promise<void> allow_first_free;
  std::shared_future<void> first_free_allowed = allow_first_free.get_future().share();
  std::atomic<uint32_t> freed_count = { 0 };

  auto make_block = [&]() { return std::make_shared<full_block_type>(); };
  {
    applied_block_releaser releaser( 1 );

    std::shared_ptr<full_block_type> first( new full_block_type(), [&]( full_block_type* block )
    {
      first_free_started.set_value();
      first_free_allowed.wait();
      delete block;
      freed_count.fetch_add( 1 );
    } );
    releaser.release( std::move( first ) );
    first_free_started.get_future().wait();

    // the releaser thread is busy freeing the first block; the second fills the queue
    releaser.release( make_block() );

    std::future<void> third_released = std::async( std::launch::async, [&]()
    {
      releaser.release( make_block() );
    } );
    const std::future_status while_full = third_released.wait_for( std::chrono::milliseconds( 200 ) );
    // unblock before checking, so a failed check does not leave the releaser thread stuck
    allow_first_free.set_value();
    BOOST_REQUIRE( while_full == std::future_status::timeout );
    BOOST_REQUIRE( third_released.wait_for( std::chrono::seconds( 30 ) ) == std::future_status::ready );
    third_released.get();
  }
  BOOST_REQUIRE_EQUAL( freed_count.load(), 1u );
}

BOOST_AUTO_TEST_SUITE_END()

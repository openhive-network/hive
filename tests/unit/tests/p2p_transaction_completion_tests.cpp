#include <boost/test/unit_test.hpp>

#include <graphene/net/node.hpp>

#include <fc/thread/thread.hpp>

#include <algorithm>
#include <atomic>
#include <numeric>
#include <thread>
#include <vector>

using graphene::net::completion_queue;
using graphene::net::in_flight_limit;

BOOST_AUTO_TEST_SUITE( p2p_transaction_completion_tests )

/// Results pushed before the consumer takes them share one scheduled take and come out in the order pushed.
BOOST_AUTO_TEST_CASE( completions_pushed_together_share_one_take )
{
  int takes_scheduled = 0;
  completion_queue<int> queue( [&]() { ++takes_scheduled; } );

  for( int i = 0; i < 5; ++i )
    queue.push( i );
  BOOST_CHECK_EQUAL( takes_scheduled, 1 );
  const std::deque<int> first = queue.take_all();
  BOOST_CHECK( first == std::deque<int>( { 0, 1, 2, 3, 4 } ) );

  queue.push( 5 );
  queue.push( 6 );
  BOOST_CHECK_EQUAL( takes_scheduled, 2 );
  BOOST_CHECK( queue.take_all() == std::deque<int>( { 5, 6 } ) );
  BOOST_CHECK( queue.take_all().empty() );
}

/// Once closed, what was not taken and every later push are discarded, and no take is scheduled.
BOOST_AUTO_TEST_CASE( closed_queue_discards_completions )
{
  int takes_scheduled = 0;
  completion_queue<int> queue( [&]() { ++takes_scheduled; } );

  queue.push( 1 );
  queue.close();
  BOOST_CHECK( queue.is_closed() );
  queue.push( 2 );
  BOOST_CHECK_EQUAL( takes_scheduled, 1 );
  BOOST_CHECK( queue.take_all().empty() );
}

/// Results pushed on another thread, one at a time like a writer thread, reach a consumer on an fc thread
/// complete and in order.
BOOST_AUTO_TEST_CASE( completions_from_another_thread_arrive_in_order )
{
  const int count = 20000;
  fc::thread consumer_thread( "consumer" );
  std::vector<int> applied;
  std::atomic<int> takes = 0;
  std::shared_ptr<completion_queue<int>> queue;
  queue = std::make_shared<completion_queue<int>>( [&]() {
    consumer_thread.async( [&]() {
      ++takes;
      for( int value : queue->take_all() )
        applied.push_back( value );
    }, "take_all" );
  } );

  std::thread producer( [&]() {
    for( int i = 0; i < count; ++i )
      queue->push( i );
  } );
  producer.join();

  const fc::time_point deadline = fc::time_point::now() + fc::seconds( 30 );
  size_t applied_count = 0;
  while( ( applied_count = consumer_thread.async( [&]() { return applied.size(); } ).wait() ) < size_t( count ) &&
         fc::time_point::now() < deadline )
    fc::usleep( fc::milliseconds( 1 ) );

  const std::vector<int> result = consumer_thread.async( [&]() { return applied; } ).wait();
  std::vector<int> expected( count );
  std::iota( expected.begin(), expected.end(), 0 );
  BOOST_CHECK_EQUAL( applied_count, size_t( count ) );
  BOOST_CHECK( result == expected );
  BOOST_TEST_MESSAGE( std::to_string( count ) + " completions took " + std::to_string( takes.load() ) + " wakes" );
  queue->close();
}

/// Tasks acquiring more than the limit wait for releases; the count never goes over the limit.
BOOST_AUTO_TEST_CASE( acquire_waits_at_the_limit )
{
  fc::thread thread( "in_flight_limit" );
  thread.async( [&]() {
    const uint32_t limit = 3;
    in_flight_limit flight;
    uint32_t highest = 0;
    int acquired = 0;
    std::vector<fc::future<void>> tasks;
    for( int i = 0; i < 10; ++i )
      tasks.push_back( fc::async( [&]() {
        flight.acquire( limit );
        ++acquired;
        highest = std::max( highest, flight.in_flight() );
      }, "acquire" ) );

    fc::usleep( fc::milliseconds( 20 ) );
    BOOST_CHECK_EQUAL( acquired, 3 );
    BOOST_CHECK_EQUAL( flight.in_flight(), limit );

    flight.release( 2 );
    fc::usleep( fc::milliseconds( 20 ) );
    BOOST_CHECK_EQUAL( acquired, 5 );

    for( int released = 5; released < 10; ++released )
    {
      flight.release( 1 );
      fc::usleep( fc::milliseconds( 5 ) );
    }
    for( fc::future<void>& task : tasks )
      task.wait();
    BOOST_CHECK_EQUAL( acquired, 10 );
    BOOST_CHECK_EQUAL( highest, limit );
  } ).wait();
}

/// A zero limit never waits.
BOOST_AUTO_TEST_CASE( zero_limit_does_not_wait )
{
  in_flight_limit flight;
  for( int i = 0; i < 100; ++i )
    flight.acquire( 0 );
  BOOST_CHECK_EQUAL( flight.in_flight(), 100u );
}

/// Closing wakes the tasks waiting for room with fc::canceled_exception, and later acquires throw it too.
BOOST_AUTO_TEST_CASE( close_wakes_waiting_tasks )
{
  fc::thread thread( "in_flight_limit" );
  thread.async( [&]() {
    in_flight_limit flight;
    flight.acquire( 1 );
    fc::future<void> waiting = fc::async( [&]() { flight.acquire( 1 ); }, "acquire" );
    fc::usleep( fc::milliseconds( 20 ) );
    BOOST_CHECK( !waiting.ready() );

    flight.close();
    BOOST_CHECK_THROW( waiting.wait(), fc::canceled_exception );
    BOOST_CHECK_THROW( flight.acquire( 0 ), fc::canceled_exception );
    BOOST_CHECK_EQUAL( flight.in_flight(), 1u );
  } ).wait();
}

BOOST_AUTO_TEST_SUITE_END()

#include <boost/test/unit_test.hpp>

#include <graphene/net/node.hpp>

#include <fc/thread/thread.hpp>

using graphene::net::coalescing_loop_trigger;

namespace {

const fc::time_point no_deadline = fc::time_point::maximum();

/// How long wait() blocks, started right after begin_iteration()
fc::microseconds time_to_wake( coalescing_loop_trigger& trigger, fc::microseconds minimum_interval, fc::time_point wake_at )
{
  const fc::time_point start = fc::time_point::now();
  trigger.wait( minimum_interval, wake_at );
  return fc::time_point::now() - start;
}

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_coalescing_loop_trigger_tests )

/// Triggers made during an iteration start the next one only once the minimum interval has passed.
BOOST_AUTO_TEST_CASE( trigger_waits_out_the_minimum_interval )
{
  coalescing_loop_trigger trigger;
  trigger.begin_iteration();
  trigger.trigger( false );
  trigger.trigger( false );
  BOOST_CHECK_GE( time_to_wake( trigger, fc::milliseconds( 50 ), no_deadline ).count(), fc::milliseconds( 45 ).count() );
}

/// With a zero interval every trigger runs the next iteration at once, as before the interval existed.
BOOST_AUTO_TEST_CASE( zero_interval_does_not_wait )
{
  coalescing_loop_trigger trigger;
  trigger.begin_iteration();
  trigger.trigger( false );
  BOOST_CHECK_LT( time_to_wake( trigger, fc::microseconds( 0 ), no_deadline ).count(), fc::milliseconds( 10 ).count() );
}

/// A trigger for a block, made before or during the wait, does not wait for the interval.
BOOST_AUTO_TEST_CASE( urgent_trigger_ends_the_wait_at_once )
{
  coalescing_loop_trigger trigger;
  trigger.begin_iteration();
  trigger.trigger( false );
  trigger.trigger( true );
  BOOST_CHECK_LT( time_to_wake( trigger, fc::seconds( 10 ), no_deadline ).count(), fc::milliseconds( 100 ).count() );

  trigger.begin_iteration();
  trigger.trigger( false );
  fc::future<void> block_advertised = fc::async( [&]() {
    fc::usleep( fc::milliseconds( 5 ) );
    trigger.trigger( true );
  }, "block_advertised" );
  BOOST_CHECK_LT( time_to_wake( trigger, fc::seconds( 10 ), no_deadline ).count(), fc::milliseconds( 500 ).count() );
  block_advertised.wait();
}

/// Without a trigger the loop sleeps until one comes; once the interval has passed, a plain trigger wakes it at once.
BOOST_AUTO_TEST_CASE( trigger_after_the_interval_wakes_the_loop )
{
  coalescing_loop_trigger trigger;
  trigger.begin_iteration();
  fc::future<void> transaction_advertised = fc::async( [&]() {
    fc::usleep( fc::milliseconds( 30 ) );
    trigger.trigger( false );
  }, "transaction_advertised" );
  const fc::microseconds waited = time_to_wake( trigger, fc::milliseconds( 5 ), no_deadline );
  BOOST_CHECK_GE( waited.count(), fc::milliseconds( 25 ).count() );
  BOOST_CHECK_LT( waited.count(), fc::milliseconds( 500 ).count() );
  transaction_advertised.wait();
}

/// The deadline (a peer whose transaction fetching stops being inhibited) wakes the loop with or without a
/// trigger, and a pending trigger does not push it later.
BOOST_AUTO_TEST_CASE( deadline_is_kept )
{
  coalescing_loop_trigger trigger;
  trigger.begin_iteration();
  fc::microseconds waited = time_to_wake( trigger, fc::seconds( 10 ), fc::time_point::now() + fc::milliseconds( 20 ) );
  BOOST_CHECK_GE( waited.count(), fc::milliseconds( 15 ).count() );
  BOOST_CHECK_LT( waited.count(), fc::milliseconds( 500 ).count() );

  trigger.begin_iteration();
  trigger.trigger( false );
  waited = time_to_wake( trigger, fc::seconds( 10 ), fc::time_point::now() + fc::milliseconds( 20 ) );
  BOOST_CHECK_GE( waited.count(), fc::milliseconds( 15 ).count() );
  BOOST_CHECK_LT( waited.count(), fc::milliseconds( 500 ).count() );
}

/// A stream of triggers runs the loop about once per interval, not once per trigger.
BOOST_AUTO_TEST_CASE( many_triggers_coalesce_into_few_iterations )
{
  coalescing_loop_trigger trigger;
  bool done = false;
  uint32_t iterations = 0;
  fc::future<void> loop = fc::async( [&]() {
    while( !done )
    {
      trigger.begin_iteration();
      ++iterations;
      trigger.wait( fc::milliseconds( 20 ), no_deadline );
    }
  }, "coalesced_loop" );

  uint32_t triggers = 0;
  const fc::time_point end = fc::time_point::now() + fc::milliseconds( 200 );
  while( fc::time_point::now() < end )
  {
    trigger.trigger( false );
    ++triggers;
    fc::usleep( fc::microseconds( 500 ) );
  }
  done = true;
  trigger.trigger( true );
  loop.wait();

  BOOST_TEST_MESSAGE( "triggers: " << triggers << ", iterations: " << iterations );
  BOOST_CHECK_GE( iterations, 2u );
  BOOST_CHECK_LE( iterations, 13u ); // 200 ms / 20 ms, plus the first and the final iteration and some slack
  BOOST_CHECK_GT( triggers, 3 * iterations );
}

/// A trigger made while an iteration runs, before the loop waits, is not lost: the next iteration starts once the
/// minimum interval has passed instead of at the deadline.
BOOST_AUTO_TEST_CASE( trigger_during_the_iteration_skips_the_deadline )
{
  coalescing_loop_trigger trigger;
  trigger.begin_iteration();
  trigger.trigger( false );
  fc::usleep( fc::milliseconds( 30 ) ); // the rest of the iteration, longer than the interval
  const fc::microseconds waited = time_to_wake( trigger, fc::milliseconds( 20 ), fc::time_point::now() + fc::seconds( 10 ) );
  BOOST_CHECK_LT( waited.count(), fc::milliseconds( 10 ).count() );
}

/// Without a trigger the deadline is the fallback; a trigger after an iteration that started more than the interval
/// ago ends the wait at once.
BOOST_AUTO_TEST_CASE( deadline_is_the_fallback_without_a_trigger )
{
  coalescing_loop_trigger trigger;
  trigger.begin_iteration();
  fc::microseconds waited = time_to_wake( trigger, fc::milliseconds( 5 ), fc::time_point::now() + fc::milliseconds( 40 ) );
  BOOST_CHECK_GE( waited.count(), fc::milliseconds( 35 ).count() );
  BOOST_CHECK_LT( waited.count(), fc::milliseconds( 500 ).count() );

  trigger.begin_iteration();
  fc::usleep( fc::milliseconds( 10 ) );
  fc::future<void> peer_added = fc::async( [&]() {
    fc::usleep( fc::milliseconds( 20 ) );
    trigger.trigger( false );
  }, "peer_added" );
  waited = time_to_wake( trigger, fc::milliseconds( 5 ), fc::time_point::now() + fc::seconds( 10 ) );
  BOOST_CHECK_GE( waited.count(), fc::milliseconds( 15 ).count() );
  BOOST_CHECK_LT( waited.count(), fc::milliseconds( 500 ).count() );
  peer_added.wait();
}

/// Canceling a loop that waits for a trigger ends it promptly with fc::canceled_exception, as node shutdown needs,
/// and triggering it after it exited is harmless.
BOOST_AUTO_TEST_CASE( canceling_a_waiting_loop_ends_it_promptly )
{
  coalescing_loop_trigger trigger;
  uint32_t iterations = 0;
  fc::future<void> loop = fc::async( [&]() {
    for( ;; )
    {
      trigger.begin_iteration();
      ++iterations;
      trigger.wait( fc::milliseconds( 1 ), fc::time_point::now() + fc::seconds( 10 ) );
    }
  }, "connect_loop" );
  fc::usleep( fc::milliseconds( 20 ) );

  const fc::time_point start = fc::time_point::now();
  loop.cancel_and_wait( "test shutdown" );
  BOOST_CHECK_LT( ( fc::time_point::now() - start ).count(), fc::milliseconds( 500 ).count() );
  BOOST_CHECK_EQUAL( iterations, 1u );

  trigger.trigger( false );
  trigger.trigger( true );
  BOOST_CHECK_EQUAL( iterations, 1u );
}

BOOST_AUTO_TEST_SUITE_END()

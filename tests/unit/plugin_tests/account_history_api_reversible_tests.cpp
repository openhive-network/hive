#if defined IS_TEST_NET

#include "condenser_api_reversible_fixture.hpp"

#include <hive/plugins/account_history_api/account_history_api.hpp>

#include <boost/scope_exit.hpp>
#include <boost/test/unit_test.hpp>

#include <pthread.h>
#include <sched.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace {

int first_cpu_of( const cpu_set_t& cpus )
{
  for( int cpu = 0; cpu < CPU_SETSIZE; ++cpu )
    if( CPU_ISSET( cpu, &cpus ) )
      return cpu;
  BOOST_FAIL( "no CPU allowed for the test thread" );
  return -1;
}

void run_on_cpu( pthread_t thread, int cpu )
{
  cpu_set_t only;
  CPU_ZERO( &only );
  CPU_SET( cpu, &only );
  FC_ASSERT( pthread_setaffinity_np( thread, sizeof( only ), &only ) == 0 );
}

} // namespace

BOOST_FIXTURE_TEST_SUITE( account_history_api_reversible_tests, condenser_api_reversible_fixture );

/**
  * get_transaction with include_reversible looks the transaction up in two steps: it scans the
  * reversible operations under the chainbase read lock to learn the block number, releases the
  * lock, and only then reads that block through the block reader. Account history API calls are
  * lockless, so the writer can pop the block in between - which is what every fork switch does
  * to the blocks of the old branch before it pushes the new ones. A popped block number is above
  * the head of both fork database and block log, so the block reader returns no block, and
  * get_transaction must not use it.
  *
  * The writer here pops the block holding the transaction and pushes the same block back, in a
  * loop and under the write lock, while the API thread keeps asking for the transaction. Every
  * answer has to be either that transaction or an error.
  *
  * Between the scan and the block read there are only a few instructions, so the API thread is
  * rarely in that window when the pop happens; on a busy API node the scheduler supplies the
  * delay. Here both threads run on one CPU: the writer, woken when the API thread releases the read
  * lock, takes the CPU, pops the block and pauses, and the API thread continues from just after its
  * scan. Without that the failure shows up only in some runs. Both threads keep the same priority on
  * purpose: with the API thread at a lower one, the writer spins on the fc logger lock that the
  * preempted API thread holds (chainbase lock calls fetch the "chainlock" logger), and every pop
  * takes about a hundred milliseconds instead of microseconds.
  */
BOOST_AUTO_TEST_CASE( get_transaction_while_its_block_is_popped )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction with include_reversible while its block is popped and pushed again" );

  transfer_operation op;
  op.from = HIVE_INIT_MINER_NAME;
  op.to = "whale";
  op.amount = ASSET( "1.000 TESTS" );
  const auto trx = push_transaction( op, init_account_priv_key );
  generate_block();

  const uint32_t block_num = db->head_block_num();
  BOOST_REQUIRE_LT( db->get_last_irreversible_block_num(), block_num );

  const auto trx_id = trx->get_transaction_id();
  const auto block = get_chain_plugin().block_reader().get_block_by_number( block_num );
  BOOST_REQUIRE( block );

  const auto reversible = account_history_api->get_transaction( { trx_id.str(), true /*include_reversible*/ } );
  BOOST_REQUIRE( reversible.transaction_id == trx_id );
  BOOST_REQUIRE_EQUAL( reversible.block_num, block_num );

  cpu_set_t original_affinity;
  BOOST_REQUIRE_EQUAL( pthread_getaffinity_np( pthread_self(), sizeof( original_affinity ), &original_affinity ), 0 );
  const int cpu = first_cpu_of( original_affinity );
  run_on_cpu( pthread_self(), cpu );

  std::atomic<bool> stop{ false };
  std::atomic<uint32_t> found{ 0 };
  std::atomic<uint32_t> rejected{ 0 };
  std::atomic<uint32_t> wrong{ 0 };

  std::thread api_thread( [&]()
  {
    run_on_cpu( pthread_self(), cpu );

    while( !stop.load( std::memory_order_relaxed ) )
    {
      try
      {
        const auto result = account_history_api->get_transaction( { trx_id.str(), true /*include_reversible*/ } );
        if( result.transaction_id == trx_id )
          ++found;
        else
          ++wrong;
      }
      catch( const fc::exception& )
      {
        ++rejected; // the block was popped when the API looked for the transaction
      }
    }
  } );

  BOOST_SCOPE_EXIT( &stop, &api_thread, &original_affinity )
  {
    stop.store( true, std::memory_order_relaxed );
    if( api_thread.joinable() )
      api_thread.join();
    pthread_setaffinity_np( pthread_self(), sizeof( original_affinity ), &original_affinity );
  } BOOST_SCOPE_EXIT_END

  const uint32_t iterations = 2000;
  const auto pause = std::chrono::microseconds( 100 );
  const auto start = std::chrono::steady_clock::now();
  for( uint32_t i = 0; i < iterations; ++i )
  {
    db->with_write_lock( [&]() { db->pop_block(); } );
    std::this_thread::sleep_for( pause ); // lets the API thread continue while the block is popped
    db->with_write_lock( [&]() { push_block( block ); } );
    BOOST_REQUIRE_EQUAL( db->head_block_num(), block_num );
    std::this_thread::sleep_for( pause ); // lets the API thread find the transaction again
  }
  const auto elapsed = std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::steady_clock::now() - start );

  stop.store( true, std::memory_order_relaxed );
  api_thread.join();

  ilog( "${i} pops of block ${b} in ${t} ms: transaction found ${f} times, rejected ${r} times, other transaction returned ${w} times",
    ( "i", iterations )( "b", block_num )( "t", elapsed.count() )( "f", found.load() )( "r", rejected.load() )( "w", wrong.load() ) );

  BOOST_REQUIRE_LT( db->get_last_irreversible_block_num(), block_num );
  BOOST_CHECK_GT( found.load(), 0u );
  BOOST_CHECK_EQUAL( wrong.load(), 0u );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()

#endif

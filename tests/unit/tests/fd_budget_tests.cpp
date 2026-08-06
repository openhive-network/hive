#include <boost/test/unit_test.hpp>

#include <hive/chain/external_storage/fd_budget.hpp>

#include <cstdint>

using namespace hive::chain;

BOOST_AUTO_TEST_SUITE( fd_budget_tests )

/// The max_open_files value handed to every RocksDB store, derived from the process descriptor
/// limit. What the finite value buys the node - table files being closed instead of accumulating
/// descriptors until writes fail - is covered where it can be observed on a running node, by
/// ah_plugin_tests/ah_rocksdb_files_do_not_track_block_count (issue #869).
BOOST_AUTO_TEST_CASE( max_open_files_budget_derivation )
{
  // Whatever the limit is, the budget is a finite positive number: never RocksDB's unbounded -1
  // and never zero, which RocksDB would clamp to its own minimum.
  for( uint64_t soft : { uint64_t( 0 ), uint64_t( 64 ), uint64_t( 1024 ), uint64_t( 8192 ),
                         uint64_t( 10240 ), uint64_t( 65536 ), uint64_t( 1048576 ) } )
    BOOST_REQUIRE_GT( max_open_files_budget_for( soft ), 0 );

  // Below the point where the headroom plus a per-store floor fits into the limit, the floor is
  // used and the result stops tracking the limit.
  BOOST_REQUIRE_EQUAL( max_open_files_budget_for( 64 ), max_open_files_budget_for( 8192 ) );

  // Above it the budget grows with the limit, yet every store this process opens plus the
  // non-RocksDB descriptors still fit inside the limit.
  const int budget_64k = max_open_files_budget_for( 65536 );
  BOOST_REQUIRE_GT( budget_64k, max_open_files_budget_for( 8192 ) );
  BOOST_REQUIRE_LT( static_cast< uint64_t >( budget_64k ) * 4, uint64_t( 65536 ) );
  BOOST_REQUIRE_LT( budget_64k, max_open_files_budget_for( 1048576 ) );

  // A limit larger than int must not wrap around into a negative max_open_files.
  BOOST_REQUIRE_GT( max_open_files_budget_for( UINT64_MAX ), 0 );

  // And the value actually handed to RocksDB is finite too.
  raise_fd_limit();
  BOOST_REQUIRE_GT( max_open_files_budget(), 0 );
}

BOOST_AUTO_TEST_SUITE_END()

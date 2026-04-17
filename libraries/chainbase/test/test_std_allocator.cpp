/**
  * Standalone test for pool_allocator_t in std::allocator mode.
  * Compiled with -DENABLE_STD_ALLOCATOR to exercise the #ifdef branches
  * that select std::allocator, std::set, std::vector, raw pointers, etc.
  *
  * Does NOT use chainbase::database — tests pool_allocator_t directly.
  */
#define BOOST_TEST_MODULE std_allocator_test
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <chainbase/pool_allocator.hpp>

using namespace chainbase;

// Verify we are actually in std::allocator mode
static_assert( _ENABLE_STD_ALLOCATOR, "This test must be compiled with ENABLE_STD_ALLOCATOR" );

BOOST_AUTO_TEST_SUITE( std_allocator_pool_tests )

BOOST_AUTO_TEST_CASE( basic_allocate_deallocate )
{
  pool_allocator_t<int, 16, false> alloc;

  int* p = &(*alloc.allocate(1));
  *p = 42;
  BOOST_CHECK_EQUAL( *p, 42 );
  BOOST_CHECK_EQUAL( alloc.get_allocated_count(), 1u );

  alloc.deallocate( p, 1 );
  BOOST_CHECK_EQUAL( alloc.get_allocated_count(), 0u );
}

BOOST_AUTO_TEST_CASE( multiple_allocations )
{
  pool_allocator_t<int, 16, false> alloc;

  constexpr int N = 16;
  int* ptrs[N];

  for( int i = 0; i < N; ++i )
  {
    ptrs[i] = &(*alloc.allocate(1));
    *ptrs[i] = i * 10;
  }

  BOOST_CHECK_EQUAL( alloc.get_allocated_count(), N );
  BOOST_CHECK_EQUAL( alloc.get_block_count(), 1u ); // all fit in one block of 16

  for( int i = 0; i < N; ++i )
    BOOST_CHECK_EQUAL( *ptrs[i], i * 10 );

  for( int i = 0; i < N; ++i )
    alloc.deallocate( ptrs[i], 1 );

  BOOST_CHECK_EQUAL( alloc.get_allocated_count(), 0u );
}

BOOST_AUTO_TEST_CASE( exceeds_block_size )
{
  pool_allocator_t<int, 4, false> alloc; // small block size to trigger multi-block

  constexpr int N = 10; // more than BLOCK_SIZE=4
  int* ptrs[N];

  for( int i = 0; i < N; ++i )
  {
    ptrs[i] = &(*alloc.allocate(1));
    *ptrs[i] = i;
  }

  BOOST_CHECK_EQUAL( alloc.get_allocated_count(), N );
  BOOST_CHECK_GE( alloc.get_block_count(), 3u ); // 10 objects / 4 per block = at least 3

  for( int i = 0; i < N; ++i )
    BOOST_CHECK_EQUAL( *ptrs[i], i );

  // Deallocate in reverse order
  for( int i = N - 1; i >= 0; --i )
    alloc.deallocate( ptrs[i], 1 );

  BOOST_CHECK_EQUAL( alloc.get_allocated_count(), 0u );
}

BOOST_AUTO_TEST_CASE( allocate_deallocate_reuse )
{
  pool_allocator_t<int, 4, false> alloc;

  // Fill a block
  int* p1 = &(*alloc.allocate(1));
  int* p2 = &(*alloc.allocate(1));
  int* p3 = &(*alloc.allocate(1));
  int* p4 = &(*alloc.allocate(1));
  BOOST_CHECK_EQUAL( alloc.get_block_count(), 1u );

  // Free two slots
  alloc.deallocate( p2, 1 );
  alloc.deallocate( p3, 1 );
  BOOST_CHECK_EQUAL( alloc.get_allocated_count(), 2u );

  // Allocate again — should reuse freed slots, not allocate new block
  int* p5 = &(*alloc.allocate(1));
  int* p6 = &(*alloc.allocate(1));
  BOOST_CHECK_EQUAL( alloc.get_allocated_count(), 4u );
  BOOST_CHECK_EQUAL( alloc.get_block_count(), 1u ); // still just one block

  alloc.deallocate( p1, 1 );
  alloc.deallocate( p4, 1 );
  alloc.deallocate( p5, 1 );
  alloc.deallocate( p6, 1 );
  BOOST_CHECK_EQUAL( alloc.get_allocated_count(), 0u );
}

BOOST_AUTO_TEST_CASE( preserve_last_block )
{
  // With PRESERVE_LAST_BLOCK=true, the block should not be released
  // even when all objects are deallocated
  pool_allocator_t<int, 4, true> alloc;

  int* p = &(*alloc.allocate(1));
  BOOST_CHECK_EQUAL( alloc.get_block_count(), 1u );

  alloc.deallocate( p, 1 );
  BOOST_CHECK_EQUAL( alloc.get_allocated_count(), 0u );
  BOOST_CHECK_EQUAL( alloc.get_block_count(), 1u ); // block preserved
}

struct large_object
{
  char data[256];
  int id;
};

BOOST_AUTO_TEST_CASE( non_trivial_type )
{
  pool_allocator_t<large_object, 8, false> alloc;

  large_object* p = &(*alloc.allocate(1));
  p->id = 999;
  std::memset( p->data, 0xAB, sizeof(p->data) );

  BOOST_CHECK_EQUAL( p->id, 999 );
  BOOST_CHECK_EQUAL( static_cast<unsigned char>(p->data[0]), 0xAB );
  BOOST_CHECK_EQUAL( static_cast<unsigned char>(p->data[255]), 0xAB );

  alloc.deallocate( p, 1 );
}

BOOST_AUTO_TEST_CASE( get_segment_manager_returns_nullptr )
{
  pool_allocator_t<int, 16, false> alloc;
  BOOST_CHECK( alloc.get_segment_manager() == nullptr );
}

BOOST_AUTO_TEST_CASE( use_managed_mapped_file_is_false )
{
  using alloc_t = pool_allocator_t<int, 16, false>;
  static_assert( !alloc_t::use_managed_mapped_file,
    "use_managed_mapped_file must be false in ENABLE_STD_ALLOCATOR mode" );
  BOOST_CHECK( true );
}

BOOST_AUTO_TEST_SUITE_END()

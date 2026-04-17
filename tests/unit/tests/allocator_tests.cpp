
#include <boost/test/unit_test.hpp>
#include <chainbase/chainbase.hpp>
#include <chainbase/chainbase.inl>

#include <fc/io/raw.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <boost/filesystem.hpp>

using namespace chainbase;
using namespace boost::multi_index;

namespace bfs = boost::filesystem;

// Use a high type number to avoid conflict with chain object types
class widget : public chainbase::object<9999, widget>
{
  CHAINBASE_OBJECT( widget );

public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( widget )

  int value = 0;
  int extra = 0;
};
typedef oid_ref< widget > widget_id_type;

typedef multi_index_container<
  widget,
  indexed_by<
    ordered_unique< tag< by_id >, const_mem_fun<widget, widget::id_type, &widget::get_id> >,
    ordered_non_unique< BOOST_MULTI_INDEX_MEMBER(widget, int, value) >
  >,
  chainbase::multi_index_allocator<widget>
> widget_index;

CHAINBASE_SET_INDEX_TYPE( widget, widget_index )

FC_REFLECT(widget, (id)(value)(extra))

namespace fc { namespace raw {
template<typename Stream>
inline void pack(Stream& s, const widget&) {}

template<typename Stream>
inline void unpack(Stream& s, widget& w, uint32_t depth = 0, bool limit_is_disabled = false) {}
}}

// Explicit template instantiations for chainbase::database methods used by widget
template const chainbase::generic_index<widget_index>& chainbase::database::get_index<widget_index>() const;
template chainbase::generic_index<widget_index>& chainbase::database::get_mutable_index<widget_index>();

/// Helper RAII struct that creates a temp dir and cleans up
struct temp_database
{
  bfs::path path;
  chainbase::database db;

  temp_database()
    : path( bfs::unique_path() )
  {
    db.open( path, 0, 1024*1024*64 ); // 64MB
    db.add_index< widget_index >();
  }

  ~temp_database()
  {
    try { bfs::remove_all( path ); } catch(...) {}
  }
};

BOOST_AUTO_TEST_SUITE( pool_allocator_tests )

BOOST_AUTO_TEST_CASE( basic_allocate_deallocate )
{
  temp_database tdb;
  auto& db = tdb.db;

  // Create an object — exercises pool_allocator_t::allocate
  const auto& w1 = db.create<widget>( []( widget& w ) {
    w.value = 10;
    w.extra = 20;
  });
  BOOST_REQUIRE_EQUAL( w1.value, 10 );
  BOOST_REQUIRE_EQUAL( w1.extra, 20 );

  // Create a second object
  const auto& w2 = db.create<widget>( []( widget& w ) {
    w.value = 30;
    w.extra = 40;
  });
  BOOST_REQUIRE_EQUAL( w2.value, 30 );

  // Remove an object — exercises pool_allocator_t::deallocate
  {
    auto session = db.start_undo_session();
    db.remove( w1 );
    session.push();
  }

  // w2 should still be accessible
  const auto& remaining = db.get<widget>( w2.get_id() );
  BOOST_REQUIRE_EQUAL( remaining.value, 30 );
}

BOOST_AUTO_TEST_CASE( many_objects_block_management )
{
  temp_database tdb;
  auto& db = tdb.db;

  // Create many objects (more than pool_allocator block size of 4096)
  // to force allocation of multiple blocks
  constexpr int NUM_OBJECTS = 5000;

  for( int i = 0; i < NUM_OBJECTS; ++i )
  {
    auto session = db.start_undo_session();
    db.create<widget>( [&]( widget& w ) {
      w.value = i;
      w.extra = i * 2;
    });
    session.push();
  }

  // Verify all objects exist
  const auto& idx = db.get_index< widget_index >();
  BOOST_REQUIRE_EQUAL( static_cast<int>(idx.indices().size()), NUM_OBJECTS );

  // Verify first and last objects
  const auto& first = db.get<widget>( widget::id_type(0) );
  BOOST_REQUIRE_EQUAL( first.value, 0 );

  const auto& last = db.get<widget>( widget::id_type(NUM_OBJECTS - 1) );
  BOOST_REQUIRE_EQUAL( last.value, NUM_OBJECTS - 1 );
}

BOOST_AUTO_TEST_CASE( undo_restores_allocator_state )
{
  temp_database tdb;
  auto& db = tdb.db;

  // Create some base objects
  for( int i = 0; i < 10; ++i )
  {
    auto session = db.start_undo_session();
    db.create<widget>( [&]( widget& w ) {
      w.value = i;
    });
    session.push();
  }

  const auto& idx = db.get_index< widget_index >();
  BOOST_REQUIRE_EQUAL( static_cast<int>(idx.indices().size()), 10 );

  // Create objects in an undo session and then undo
  {
    auto session = db.start_undo_session();
    for( int i = 0; i < 100; ++i )
    {
      db.create<widget>( [&]( widget& w ) {
        w.value = 100 + i;
      });
    }
    BOOST_REQUIRE_EQUAL( static_cast<int>(idx.indices().size()), 110 );
    // session destructor triggers undo — all 100 new objects should be removed
  }

  BOOST_REQUIRE_EQUAL( static_cast<int>(idx.indices().size()), 10 );

  // Verify original objects are intact
  for( int i = 0; i < 10; ++i )
  {
    const auto& w = db.get<widget>( widget::id_type(i) );
    BOOST_REQUIRE_EQUAL( w.value, i );
  }
}

BOOST_AUTO_TEST_CASE( undo_remove_then_undo )
{
  temp_database tdb;
  auto& db = tdb.db;

  // Create objects
  for( int i = 0; i < 5; ++i )
  {
    auto session = db.start_undo_session();
    db.create<widget>( [&]( widget& w ) {
      w.value = i * 10;
    });
    session.push();
  }

  // Remove objects in an undo session, then undo to restore them
  {
    auto session = db.start_undo_session();
    for( int i = 4; i >= 0; --i )
    {
      const auto& w = db.get<widget>( widget::id_type(i) );
      db.remove( w );
    }

    const auto& idx = db.get_index< widget_index >();
    BOOST_REQUIRE_EQUAL( static_cast<int>(idx.indices().size()), 0 );
    // session destructor triggers undo — all 5 objects should be restored
  }

  // Verify all objects are back
  for( int i = 0; i < 5; ++i )
  {
    const auto& w = db.get<widget>( widget::id_type(i) );
    BOOST_REQUIRE_EQUAL( w.value, i * 10 );
  }
}

BOOST_AUTO_TEST_CASE( modify_and_undo )
{
  temp_database tdb;
  auto& db = tdb.db;

  auto session = db.start_undo_session();
  const auto& w = db.create<widget>( []( widget& w ) {
    w.value = 100;
    w.extra = 200;
  });
  session.push();

  // Modify in an undo session, then undo
  {
    auto session2 = db.start_undo_session();
    db.modify( w, []( widget& w ) {
      w.value = 999;
      w.extra = 888;
    });
    BOOST_REQUIRE_EQUAL( w.value, 999 );
    BOOST_REQUIRE_EQUAL( w.extra, 888 );
    // undo
  }

  BOOST_REQUIRE_EQUAL( w.value, 100 );
  BOOST_REQUIRE_EQUAL( w.extra, 200 );
}

BOOST_AUTO_TEST_CASE( repeated_create_remove_cycles )
{
  temp_database tdb;
  auto& db = tdb.db;

  // Repeatedly create and remove objects to exercise memory reuse
  for( int cycle = 0; cycle < 20; ++cycle )
  {
    std::vector<widget::id_type> ids;

    {
      auto session = db.start_undo_session();
      for( int i = 0; i < 50; ++i )
      {
        const auto& w = db.create<widget>( [&]( widget& w ) {
          w.value = cycle * 1000 + i;
        });
        ids.push_back( w.get_id() );
      }
      session.push();
    }

    // Remove all objects we just created
    {
      auto session = db.start_undo_session();
      for( auto& id : ids )
      {
        const auto& w = db.get<widget>( id );
        db.remove( w );
      }
      session.push();
    }
  }

  // After all cycles, index should be empty
  const auto& idx = db.get_index< widget_index >();
  BOOST_REQUIRE_EQUAL( static_cast<int>(idx.indices().size()), 0 );
}

BOOST_AUTO_TEST_CASE( large_batch_stress )
{
  temp_database tdb;
  auto& db = tdb.db;

  // Create a large batch of objects
  constexpr int BATCH_SIZE = 10000;

  {
    auto session = db.start_undo_session();
    for( int i = 0; i < BATCH_SIZE; ++i )
    {
      db.create<widget>( [&]( widget& w ) {
        w.value = i;
        w.extra = BATCH_SIZE - i;
      });
    }
    session.push();
  }

  const auto& idx = db.get_index< widget_index >();
  BOOST_REQUIRE_EQUAL( static_cast<int>(idx.indices().size()), BATCH_SIZE );

  // Verify a few random elements via secondary index
  const auto& by_value_idx = idx.indices().template get<1>();
  auto it = by_value_idx.find( 42 );
  BOOST_REQUIRE( it != by_value_idx.end() );
  BOOST_REQUIRE_EQUAL( it->value, 42 );
  BOOST_REQUIRE_EQUAL( it->extra, BATCH_SIZE - 42 );

  // Remove half of the objects (even-valued)
  {
    auto session = db.start_undo_session();
    for( int i = 0; i < BATCH_SIZE; i += 2 )
    {
      const auto& w = db.get<widget>( widget::id_type(i) );
      db.remove( w );
    }
    session.push();
  }

  BOOST_REQUIRE_EQUAL( static_cast<int>(idx.indices().size()), BATCH_SIZE / 2 );

  // Remaining objects should all have odd ids
  for( int i = 1; i < BATCH_SIZE; i += 2 )
  {
    const auto& w = db.get<widget>( widget::id_type(i) );
    BOOST_REQUIRE_EQUAL( w.value, i );
  }
}

BOOST_AUTO_TEST_CASE( nested_undo_sessions )
{
  temp_database tdb;
  auto& db = tdb.db;

  // Create a base object
  {
    auto session = db.start_undo_session();
    db.create<widget>( []( widget& w ) { w.value = 1; });
    session.push();
  }

  // Nested undo sessions
  {
    auto session1 = db.start_undo_session();
    db.create<widget>( []( widget& w ) { w.value = 2; });
    session1.push();

    {
      auto session2 = db.start_undo_session();
      db.create<widget>( []( widget& w ) { w.value = 3; });
      session2.push();

      {
        auto session3 = db.start_undo_session();
        db.create<widget>( []( widget& w ) { w.value = 4; });
        // session3 not pushed — undo
      }

      const auto& idx = db.get_index< widget_index >();
      BOOST_REQUIRE_EQUAL( static_cast<int>(idx.indices().size()), 3 );
    }

    // session2 was pushed, so undo pops session2
    db.undo();

    const auto& idx = db.get_index< widget_index >();
    BOOST_REQUIRE_EQUAL( static_cast<int>(idx.indices().size()), 2 );
  }

  // session1 was pushed, undo pops session1
  db.undo();

  const auto& idx = db.get_index< widget_index >();
  BOOST_REQUIRE_EQUAL( static_cast<int>(idx.indices().size()), 1 );

  const auto& w = db.get<widget>( widget::id_type(0) );
  BOOST_REQUIRE_EQUAL( w.value, 1 );
}

BOOST_AUTO_TEST_CASE( allocator_mode_consistency )
{
  // Verify the allocator is using the expected mode based on ENABLE_STD_ALLOCATOR
  using alloc_type = chainbase::multi_index_allocator<widget>;
#if defined(ENABLE_STD_ALLOCATOR)
  // In std::allocator mode, USE_MANAGED_MAPPED_FILE should be false
  static_assert( !alloc_type::use_managed_mapped_file,
    "Expected USE_MANAGED_MAPPED_FILE=false when ENABLE_STD_ALLOCATOR is defined" );
  BOOST_TEST_MESSAGE( "Running with std::allocator (ENABLE_STD_ALLOCATOR mode)" );
#else
  // In managed_mapped_file mode, USE_MANAGED_MAPPED_FILE should be true
  static_assert( alloc_type::use_managed_mapped_file,
    "Expected USE_MANAGED_MAPPED_FILE=true when ENABLE_STD_ALLOCATOR is not defined" );
  BOOST_TEST_MESSAGE( "Running with bip::allocator (managed_mapped_file mode)" );
#endif
  BOOST_CHECK( true ); // test passes if static_assert above compiles
}

BOOST_AUTO_TEST_SUITE_END()

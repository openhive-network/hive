#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>

BOOST_AUTO_TEST_SUITE( variant_tests )

// Eliminates code redundancy wrapping the actual tests in error handling and logging
#define HIVE_AUTO_TEST_CASE( name, ... )           \
  BOOST_AUTO_TEST_CASE( name )                     \
  {                                                \
    try                                            \
    {                                              \
      BOOST_TEST_MESSAGE( "--- Testing: " #name ); \
      __VA_ARGS__                                  \
    }                                              \
    FC_LOG_AND_RETHROW()                           \
  }

// Pre-defined types

HIVE_AUTO_TEST_CASE( as_int64,
  int64_t initial = -1;
  
  fc::variant v = initial;

  BOOST_REQUIRE_EQUAL( v.as_int64(), initial );
)

HIVE_AUTO_TEST_CASE( as_uint64,
  uint64_t initial = -1;
  
  fc::variant v = initial;

  BOOST_REQUIRE_EQUAL( v.as_uint64(), initial );
)

HIVE_AUTO_TEST_CASE( as_bool,
  fc::variant v = true;
  BOOST_REQUIRE_EQUAL( v.as_bool(), true );

  // Make sure that true is not a false-positive result
  v = false;
  BOOST_REQUIRE_EQUAL( v.as_bool(), false );
)

HIVE_AUTO_TEST_CASE( as_double,
  double initial = 3.141592;
  
  fc::variant v = initial;

  BOOST_REQUIRE_EQUAL( v.as_double(), initial );
)

HIVE_AUTO_TEST_CASE( as_blob,
  fc::blob initial;
  initial.data = { 'a', 'l', 'i', 'c', 'e' };
  
  fc::variant v = initial;
  fc::blob out = v.as_blob();

  BOOST_REQUIRE_EQUAL( out.data.size(), initial.data.size() );

  for( size_t i = 0; i < out.data.size(); ++i )
    BOOST_REQUIRE_EQUAL( out.data.at(i), initial.data.at(i) );
)

// TODO: as_string, as_array, as_object and extended nested object tests along with the variant_object and mutable_variant_object tests

BOOST_AUTO_TEST_SUITE_END()
#endif

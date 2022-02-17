#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <fc/variant.hpp>

BOOST_FIXTURE_TEST_SUITE( variant_tests )

// Pre-defined types

BOOST_AUTO_TEST_CASE( as_int64 )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: as_int64" );
    int64_t initial = -1;
    
    fc::variant v = initial;

    BOOST_REQUIRE_EQUAL( v.as_int64(), initial );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( as_uint64 )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: as_uint64" );
    uint64_t initial = -1;
    
    fc::variant v = initial;

    BOOST_REQUIRE_EQUAL( v.as_uint64(), initial );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( as_bool )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: as_bool" );
    bool initial = true;
    
    fc::variant v = initial;

    BOOST_REQUIRE_EQUAL( v.as_bool(), initial );

    // Make sure that true is not a false-positive result
    initial = false;
    v = initial;

    BOOST_REQUIRE_EQUAL( v.as_bool(), initial );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( as_double )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: as_double" );
    double initial = 3.141592;
    
    fc::variant v = initial;

    BOOST_REQUIRE_EQUAL( v.as_double(), initial );
  }
  FC_LOG_AND_RETHROW()
}

// TODO: as_blob, as_string, as_array, as_object and extended nested object tests along with the variant_object and mutable_variant_object tests

BOOST_AUTO_TEST_SUITE_END()
#endif

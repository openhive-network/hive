#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>

#include <string>
#include <functional>
#include <limits>

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
  BOOST_REQUIRE_EQUAL( v.get_type(), fc::variant::int64_type );

  BOOST_REQUIRE_EQUAL( v.as_int64(), initial );
)

HIVE_AUTO_TEST_CASE( as_uint64,
  uint64_t initial = -1;

  fc::variant v = initial;
  BOOST_REQUIRE_EQUAL( v.get_type(), fc::variant::uint64_type );

  BOOST_REQUIRE_EQUAL( v.as_uint64(), initial );
)

HIVE_AUTO_TEST_CASE( as_bool,
  fc::variant v = true;
  BOOST_REQUIRE_EQUAL( v.get_type(), fc::variant::bool_type );
  BOOST_REQUIRE_EQUAL( v.as_bool(), true );

  // Make sure that true is not a false-positive result
  v = false;
  BOOST_REQUIRE_EQUAL( v.as_bool(), false );
)

HIVE_AUTO_TEST_CASE( as_double,
  double initial = 3.141592;

  fc::variant v = initial;
  BOOST_REQUIRE_EQUAL( v.get_type(), fc::variant::double_type );

  BOOST_REQUIRE_EQUAL( v.as_double(), initial );
)

HIVE_AUTO_TEST_CASE( as_blob,
  fc::blob initial;
  initial.data = { 'a', 'l', 'i', 'c', 'e' };

  fc::variant v = initial;
  fc::blob out = v.as_blob();
  BOOST_REQUIRE_EQUAL( v.get_type(), fc::variant::blob_type );

  BOOST_REQUIRE_EQUAL( out.data.size(), initial.data.size() );

  for( size_t i = 0; i < out.data.size(); ++i )
    BOOST_REQUIRE_EQUAL( out.data.at(i), initial.data.at(i) );
)

HIVE_AUTO_TEST_CASE( as_string,
  std::string initial = "alice";

  fc::variant v = initial;
  BOOST_REQUIRE_EQUAL( v.get_type(), fc::variant::string_type );

  BOOST_REQUIRE_EQUAL( v.as_string(), initial );
)

HIVE_AUTO_TEST_CASE( null_type,
  fc::variant v;
  BOOST_REQUIRE_EQUAL( v.get_type(), fc::variant::null_type );

  v = 3.141592;
  v.clear();
  BOOST_REQUIRE_EQUAL( v.get_type(), fc::variant::null_type );
)

// Type conversions

namespace
{

template< typename TypeTested >
using variant_retrieve_member_fn_t = TypeTested (fc::variant::*)() const;

#define HIVE_TEST_VARIANT_FUNCTION_INVOKE(object, member_ptr) ((object).*(member_ptr))()

template< typename TypeTested >
void test_type_conversion( variant_retrieve_member_fn_t< TypeTested > variant_retrieve_fn )
{
  fc::variant v;
  TypeTested expected = std::numeric_limits< unsigned >::max();

  const auto type_conversion_helper = [&]( auto&& input ) {
    v = input;
    BOOST_REQUIRE_EQUAL( (HIVE_TEST_VARIANT_FUNCTION_INVOKE( v, variant_retrieve_fn )), expected );
  };

  type_conversion_helper( std::to_string( expected ) );
  type_conversion_helper( double( expected ) );
  type_conversion_helper( uint64_t( expected ) );
  type_conversion_helper( int64_t( expected ) );

  expected = true;
  type_conversion_helper( bool( expected ) );
  expected = false;
  type_conversion_helper( bool( expected ) );

  v.clear();
  BOOST_REQUIRE_EQUAL( (HIVE_TEST_VARIANT_FUNCTION_INVOKE( v, variant_retrieve_fn )), 0 );

  // Objects:
  v = fc::variant_object{};
  BOOST_REQUIRE_THROW( (HIVE_TEST_VARIANT_FUNCTION_INVOKE( v, variant_retrieve_fn )), fc::bad_cast_exception );
  v = fc::mutable_variant_object{};
  BOOST_REQUIRE_THROW( (HIVE_TEST_VARIANT_FUNCTION_INVOKE( v, variant_retrieve_fn )), fc::bad_cast_exception );

  // Arrays:
  v = fc::variants{};
  BOOST_REQUIRE_THROW( (HIVE_TEST_VARIANT_FUNCTION_INVOKE( v, variant_retrieve_fn )), fc::bad_cast_exception );

  // Blob:
  v = fc::blob{};
  BOOST_REQUIRE_THROW( (HIVE_TEST_VARIANT_FUNCTION_INVOKE( v, variant_retrieve_fn )), fc::bad_cast_exception );
}

}

HIVE_AUTO_TEST_CASE( as_int64_conversions,
  test_type_conversion< int64_t >( &fc::variant::as_int64 );
)

HIVE_AUTO_TEST_CASE( as_uint64_conversions,
  test_type_conversion< uint64_t >( &fc::variant::as_uint64 );
)

HIVE_AUTO_TEST_CASE( as_double_conversions,
  test_type_conversion< double >( &fc::variant::as_double );
)

HIVE_AUTO_TEST_CASE( as_bool_conversions,
  bool expected = true;
  fc::variant v;

  const auto type_conversion_helper = [&]( auto&& input ) {
    v = input;
    BOOST_REQUIRE_EQUAL( v.as_bool(), expected );
  };

  type_conversion_helper( uint64_t( expected ) );
  type_conversion_helper( int64_t( expected ) );
  type_conversion_helper( double( expected ) );

  type_conversion_helper( std::string{ "true" } );

  v.clear();
  BOOST_REQUIRE_EQUAL( v.as_bool(), false );

  expected = false;
  type_conversion_helper( uint64_t( expected ) );
  type_conversion_helper( int64_t( expected ) );
  type_conversion_helper( double( expected ) );

  type_conversion_helper( std::string{ "false" } );

  // Objects:
  v = fc::variant_object{};
  BOOST_REQUIRE_THROW( v.as_bool(), fc::bad_cast_exception );
  v = fc::mutable_variant_object{};
  BOOST_REQUIRE_THROW( v.as_bool(), fc::bad_cast_exception );

  // Arrays:
  v = fc::variants{};
  BOOST_REQUIRE_THROW( v.as_bool(), fc::bad_cast_exception );

  // Blob:
  v = fc::blob{};
  BOOST_REQUIRE_THROW( v.as_bool(), fc::bad_cast_exception );
)

// TODO: conversion, as_array, as_object and extended nested object tests along with the variant_object and mutable_variant_object tests

BOOST_AUTO_TEST_SUITE_END()
#endif
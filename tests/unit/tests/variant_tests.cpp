#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <fc/variant.hpp>
#include <fc/variant_object.hpp>
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

HIVE_AUTO_TEST_CASE( mutable_variant_object,
  fc::variant_object vo;
  BOOST_REQUIRE_EQUAL( vo.size(), 0 );

  fc::mutable_variant_object mvo;
  vo = mvo( "a", 1 )( "b", true ).set( "c", 3.14 );
  BOOST_REQUIRE_EQUAL( mvo[ "a" ].get_type(), fc::variant::int64_type );
  BOOST_REQUIRE_EQUAL( mvo[ "b" ].get_type(), fc::variant::bool_type );
  BOOST_REQUIRE_EQUAL( mvo[ "c" ].get_type(), fc::variant::double_type );

  BOOST_REQUIRE_EQUAL( vo.size(), 3 );

  // Throws on const object as it cannot add another element to the entries storage
  BOOST_REQUIRE_THROW( static_cast< const fc::mutable_variant_object >( mvo )[ "d" ], fc::key_not_found_exception );
  // But inserts variant to the storage if used on non-const object
  BOOST_REQUIRE_NO_THROW( mvo[ "d" ] );

  BOOST_REQUIRE_EQUAL( mvo.size(), 4 );

  BOOST_REQUIRE_EQUAL( mvo[ "a" ].as_int64(),  vo[ "a" ].as_int64() );
  BOOST_REQUIRE_EQUAL( mvo[ "b" ].as_bool(),   vo[ "b" ].as_bool() );
  BOOST_REQUIRE_EQUAL( mvo[ "c" ].as_double(), vo[ "c" ].as_double() );
  BOOST_REQUIRE_EQUAL( mvo[ "d" ].get_type(), fc::variant::null_type );

  // Random value test
  BOOST_REQUIRE_EQUAL( mvo[ "c" ].as_double(), 3.14 );
)

HIVE_AUTO_TEST_CASE( variant_object,
  fc::variant_object vo;
  BOOST_REQUIRE_EQUAL( vo.size(), 0 );

  fc::mutable_variant_object mvo;
  vo = mvo( "a", 1 )( "b", true ).set( "c", 3.14 );

  BOOST_REQUIRE_EQUAL( vo.size(), 3 );

  BOOST_REQUIRE( vo.contains( "c" ) );

  BOOST_REQUIRE_EQUAL( vo[ "a" ].get_type(), fc::variant::int64_type );
  BOOST_REQUIRE_EQUAL( vo[ "b" ].get_type(), fc::variant::bool_type );
  BOOST_REQUIRE_EQUAL( vo[ "c" ].get_type(), fc::variant::double_type );

  BOOST_REQUIRE_THROW( vo[ "d" ], fc::key_not_found_exception );

  // Random value test
  BOOST_REQUIRE_EQUAL( vo[ "c" ].as_double(), 3.14 );
)

HIVE_AUTO_TEST_CASE( object_type,
  fc::variant v = fc::variant_object{ "a", 3.14 };
  BOOST_REQUIRE_EQUAL( v.get_type(), fc::variant::object_type );

  const auto vo = v.get_object();
  BOOST_REQUIRE_EQUAL( vo.size(), 1 );

  BOOST_REQUIRE_EQUAL( vo.find( "a" )->value().get_type(), fc::variant::double_type );

  BOOST_REQUIRE_EQUAL( vo.find( "a" )->value().as_double(), vo.begin()->value().as_double() );

  BOOST_REQUIRE_EQUAL( vo[ "a" ].get_type(), fc::variant::double_type );

  // Test variant [] operator
  BOOST_REQUIRE_EQUAL( v[ "a" ].get_type(), fc::variant::double_type );

  BOOST_REQUIRE_EQUAL( v[ "a" ].as_double(), 3.14 );
)

HIVE_AUTO_TEST_CASE( array_type,
  fc::variants vs = {
    { 1 },
    { true },
    { 3.14 }
  };

  fc::variant v = vs;
  BOOST_REQUIRE_EQUAL( v.get_type(), fc::variant::array_type );
  BOOST_REQUIRE_EQUAL( v.size(), 3 );

  const auto variants_out = v.get_array();
  BOOST_REQUIRE_EQUAL( variants_out.size(), 3 );

  BOOST_REQUIRE_EQUAL( variants_out[ 0 ].get_type(), fc::variant::int64_type );
  BOOST_REQUIRE_EQUAL( variants_out[ 1 ].get_type(), fc::variant::bool_type );
  BOOST_REQUIRE_EQUAL( variants_out[ 2 ].get_type(), fc::variant::double_type );

  // Test variant [] operator
  BOOST_REQUIRE_EQUAL( v[ 1 ].get_type(), fc::variant::bool_type );

  // Random value test
  BOOST_REQUIRE_EQUAL( v[ 2 ].as_double(), 3.14 );
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

  BOOST_REQUIRE_THROW( type_conversion_helper( std::string{ "alice" } ), fc::bad_cast_exception );

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

HIVE_AUTO_TEST_CASE( as_string_conversions,
  const uint64_t expected_integer = 123;
  std::string expected = std::to_string( expected_integer );
  fc::variant v;

  const auto type_conversion_helper = [&]( auto&& input ) {
    v = input;
    BOOST_REQUIRE_EQUAL( v.as_string(), expected );
  };

  type_conversion_helper( uint64_t( expected_integer ) );
  type_conversion_helper( int64_t( expected_integer ) );

  expected = std::to_string( double( expected_integer ) );
  v = double( expected_integer );
  BOOST_REQUIRE_EQUAL( v.as_string().substr( 0, expected.size() ), expected );

  expected = "true";
  type_conversion_helper( true );
  expected = "false";
  type_conversion_helper( false );

  v.clear();
  BOOST_REQUIRE( v.as_string().empty() );

  // Objects:
  v = fc::variant_object{};
  BOOST_REQUIRE_THROW( v.as_string(), fc::bad_cast_exception );
  v = fc::mutable_variant_object{};
  BOOST_REQUIRE_THROW( v.as_string(), fc::bad_cast_exception );

  // Arrays:
  v = fc::variants{};
  BOOST_REQUIRE_THROW( v.as_string(), fc::bad_cast_exception );

  // Blob:
  fc::blob blob;
  blob.data = { 'a', 'l', 'i', 'c', 'e' };
  v = blob;
  BOOST_REQUIRE_EQUAL( v.as_string(), "YWxpY2U==" );
)

HIVE_AUTO_TEST_CASE( as_blob_conversions,
  fc::blob blob;
  blob.data = { 'a', 'l', 'i', 'c', 'e' };
  fc::variant v = "YWxpY2U==";

  const auto compare_blob = []( const auto& lhs, const auto& rhs ) {
    BOOST_REQUIRE_EQUAL( lhs.data.size(), rhs.data.size() );

    for( size_t i = 0; i < lhs.data.size(); ++i )
      BOOST_REQUIRE_EQUAL( lhs.data.at(i), rhs.data.at(i) );
  };

  compare_blob( v.as_blob(), blob );

  v = std::string{ "alice", 5 };
  compare_blob( v.as_blob(), blob );

  v.clear();
  BOOST_REQUIRE_EQUAL( v.as_blob().data.size(), 0 );

  const auto create_vector = []( auto&& input ) {
    return std::vector<char>( (char*)&input, (char*)&input + sizeof(input) );
  };

  const auto create_and_compare = [&]( auto&& input ) {
    v = input;
    blob.data = create_vector( input );
    compare_blob( v.as_blob(), blob );
  };

  create_and_compare( int64_t( -1 ) );
  create_and_compare( uint64_t( -1 ) );
  create_and_compare( double( -1 ) );

  // Objects:
  v = fc::variant_object{};
  BOOST_REQUIRE_THROW( v.as_string(), fc::bad_cast_exception );
  v = fc::mutable_variant_object{};
  BOOST_REQUIRE_THROW( v.as_string(), fc::bad_cast_exception );

  // Arrays:
  v = fc::variants{};
  BOOST_REQUIRE_THROW( v.as_string(), fc::bad_cast_exception );
)

// Test variant construction from non-standard types
HIVE_AUTO_TEST_CASE( extended_variant_construction,
  using fc::variant;
  variant v;

  const auto test_type = [&]( auto&& value, variant::type_id type ) {
    v = { value };
    BOOST_REQUIRE_EQUAL( v.get_type(), type );
  };

  test_type( nullptr_t{}, variant::null_type );

  char c_arr[6] = { 'a', 'l', 'i', 'c', 'e', '\0' };
  test_type( c_arr, variant::string_type );
  test_type( static_cast< const char* >( c_arr ), variant::string_type );

  wchar_t wc_arr[6] = { 'a', 'l', 'i', 'c', 'e', '\0' };
  test_type( wc_arr, variant::string_type );
  test_type( static_cast< const wchar_t* >( wc_arr ), variant::string_type );

  test_type( fc::string{ c_arr }, variant::string_type );

  test_type( float{}, variant::double_type );

  test_type( int8_t{}, variant::int64_type );
  test_type( int16_t{}, variant::int64_type );
  test_type( int32_t{}, variant::int64_type );

  test_type( uint8_t{}, variant::uint64_type );
  test_type( uint16_t{}, variant::uint64_type );
  test_type( uint32_t{}, variant::uint64_type );
)

// TODO: operators, extended nested object tests (from_variant, to_variant, as etc.)

BOOST_AUTO_TEST_SUITE_END()
#endif
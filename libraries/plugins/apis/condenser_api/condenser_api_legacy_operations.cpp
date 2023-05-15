#include <hive/plugins/condenser_api/condenser_api_legacy_operations.hpp>

#define LEGACY_PREFIX "legacy_"
#define LEGACY_PREFIX_OFFSET (7)

namespace fc {

bool obsolete_call_detector::enable_obsolete_call_detection = true;

std::string name_from_legacy_type( const std::string& type_name )
{
  auto start = type_name.find( LEGACY_PREFIX );
  if( start == std::string::npos )
  {
    start = type_name.find_last_of( ':' ) + 1;
  }
  else
  {
    start += LEGACY_PREFIX_OFFSET;
  }
  auto end   = type_name.find_last_of( '_' );

  return type_name.substr( start, end-start );
}

struct from_operation
{
  variant& var;
  from_operation( variant& dv )
    : var( dv ) {}

  typedef void result_type;
  template<typename T> void operator()( const T& v )const
  {
    auto name = name_from_legacy_type( fc::get_typename< T >::name() );
    var = variant( std::make_pair( name, v ) );
  }
};

struct get_operation_name
{
  string& name;
  get_operation_name( string& dv )
    : name( dv ) {}

  typedef void result_type;
  template< typename T > void operator()( const T& v )const
  {
    name = name_from_legacy_type( fc::get_typename< T >::name() );
  }
};

} // fc

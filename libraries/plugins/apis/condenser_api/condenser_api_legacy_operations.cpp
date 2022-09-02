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

void to_variant( const hive::plugins::condenser_api::legacy_operation& var,  fc::variant& vo )
{
  var.visit( from_operation( vo ) );
}

void from_variant( const fc::variant& var, hive::plugins::condenser_api::legacy_operation& vo )
{
  static std::map<string,int64_t> to_tag = []()
  {
    std::map<string,int64_t> name_map;
    for( int i = 0; i < hive::plugins::condenser_api::legacy_operation::count(); ++i )
    {
      hive::plugins::condenser_api::legacy_operation tmp;
      tmp.set_which(i);
      string n;
      tmp.visit( get_operation_name(n) );
      name_map[n] = i;
    }
    return name_map;
  }();
  static std::map<string,int64_t> to_full_tag = []()
  {
    std::map<string,int64_t> name_map;
    for( int i = 0; i < hive::plugins::condenser_api::legacy_operation::count(); ++i )
    {
      hive::plugins::condenser_api::legacy_operation tmp;
      tmp.set_which(i);
      string n;
      tmp.visit( get_static_variant_name(n) );
      name_map[n] = i;
    }
    return name_map;
  }();

  auto ar = var.get_array();
  if( ar.size() < 2 ) return;
  if( ar[0].is_uint64() )
    vo.set_which( ar[0].as_uint64() );
  else
  {
    auto itr = to_tag.find(ar[0].as_string());
    if( itr == to_tag.end() )
    {
        itr = to_full_tag.find(ar[0].as_string());
        FC_ASSERT( itr != to_full_tag.end(), "Invalid operation name: ${n}", ("n", ar[0]) );
    }
    vo.set_which( itr->second );
  }

  try
  {
    vo.visit( fc::to_static_variant( ar[1] ) );
  }
  catch( fc::bad_cast_exception& e )
  {
    /*
      Sometimes an operation has an extension( it's a static variant ) which is given in `legacy` format.
      For example: `comment_options` has `legacy_comment_options_extensions`.
      Therefore forcing a legacy mode is needed, because a static variant is incorrectly processed( haf26 is set by default ).
    */
    mode_guard guard( hive::protocol::transaction_serialization_type::legacy );
    ilog("Change of serialization( `${method_name}' ) - a legacy format is enabled now",("method_name", ar[0]) );

    vo.visit( fc::to_static_variant( ar[1] ) );
  }
}

} // fc

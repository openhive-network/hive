
#pragma once

#include <hive/schema/abstract_schema.hpp>
#include <hive/schema/schema_impl.hpp>

#include <hive/protocol/fixed_string.hpp>

namespace hive { namespace schema { namespace detail {

//////////////////////////////////////////////
// fixed_string                             //
//////////////////////////////////////////////

template< size_t N >
struct schema_fixed_string_impl
  : public abstract_schema
{
  HIVE_SCHEMA_TEMPLATE_CLASS_BODY( schema_fixed_string_impl )
};

template< size_t N >
void schema_fixed_string_impl<N>::get_deps( std::vector< std::shared_ptr< abstract_schema > >& deps )
{
  return;
}

template< size_t N >
void schema_fixed_string_impl<N>::get_str_schema( std::string& s )
{
  if( str_schema != "" )
  {
    s = str_schema;
    return;
  }

  std::string my_name;
  get_name( my_name );
  fc::mutable_variant_object mvo;
  mvo("name", my_name)
    ("type", "string")
    ("max_size", N)
    ;

  str_schema = fc::json::to_string( mvo );
  s = str_schema;
  return;
}

}

template< size_t N >
struct schema_reflect< typename hive::protocol::fixed_string_impl_for_size<N> >
{
  typedef detail::schema_fixed_string_impl< N >        schema_impl_type;
};

} }

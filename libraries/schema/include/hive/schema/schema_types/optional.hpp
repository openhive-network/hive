
#pragma once

#include <hive/schema/abstract_schema.hpp>
#include <hive/schema/schema_impl.hpp>

#include <fc/optional.hpp>

#include <utility>

namespace hive { namespace schema { namespace detail {

//////////////////////////////////////////////
// optional                                 //
//////////////////////////////////////////////

template< typename E >
struct schema_optional_impl
  : public abstract_schema
{
  HIVE_SCHEMA_TEMPLATE_CLASS_BODY( schema_optional_impl )
};

template< typename E >
void schema_optional_impl< E >::get_deps( std::vector< std::shared_ptr< abstract_schema > >& deps )
{
  deps.push_back( get_schema_for_type<E>() );
}

template< typename E >
void schema_optional_impl< E >::get_str_schema( std::string& s )
{
  if( str_schema != "" )
  {
    s = str_schema;
    return;
  }

  std::vector< std::shared_ptr< abstract_schema > > deps;
  get_deps( deps );
  std::string e_type;
  deps[0]->get_name(e_type);

  std::string my_name;
  get_name( my_name );
  fc::variant_object_builder mvo = fc::variant_object_builder
    ("name", my_name)
    ("type", "optional")
    ("etype", e_type)
    ;

  str_schema = fc::json::to_string( mvo.get() );
  s = str_schema;
  return;
}

}

template< typename E >
struct schema_reflect< fc::optional< E > >
{
  typedef detail::schema_optional_impl< E >        schema_impl_type;
};

} }

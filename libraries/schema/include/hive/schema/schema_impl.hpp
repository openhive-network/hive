
#pragma once

#include <fc/variant_object.hpp>

#include <fc/io/json.hpp>

#include <fc/reflect/reflect.hpp>

#include <memory>
#include <string>
#include <vector>

namespace hive { namespace schema { namespace detail {

struct get_deps_member_visitor
{
  get_deps_member_visitor( std::vector< std::shared_ptr< abstract_schema > >& deps ) : _deps(deps) {}

  typedef void result_type;

  template<typename Member, class Class, Member (Class::*member)>
  void operator()( const char* name )const
  {
    _deps.push_back( get_schema_for_type<Member>() );
  }

  std::vector< std::shared_ptr< abstract_schema > >& _deps;
};

struct get_str_schema_class_member_visitor
{
  get_str_schema_class_member_visitor() {}

  typedef void result_type;

  template<typename Member, class Class, Member (Class::*member)>
  void operator()( const char* name )const
  {
    std::shared_ptr< abstract_schema > member_schema = get_schema_for_type<Member>();
    _members.emplace_back();
    member_schema->get_name( _members.back().first );
    _members.back().second = name;
  }

  mutable std::vector< std::pair< std::string, std::string > > _members;
};

struct get_str_schema_enum_member_visitor
{
  get_str_schema_enum_member_visitor() {}

  typedef void result_type;

  template< typename Enum >
  void operator()( const char* name, Enum value )const
  {
    _members.emplace_back( name, int64_t(value) );
  }

  mutable std::vector< std::pair< std::string, int64_t > > _members;
};

#define HIVE_DECLARE_SCHEMA_CLASS( is_reflected, is_enum )  \
template< typename ObjectType >                                 \
struct schema_impl< ObjectType, is_reflected, is_enum >         \
  : public abstract_schema                                     \
{                                                               \
HIVE_SCHEMA_TEMPLATE_CLASS_BODY( schema_impl )              \
};

// Templated version of macro includes some method definitions
#define HIVE_SCHEMA_TEMPLATE_CLASS_BODY( CLASSNAME )        \
  CLASSNAME( int64_t id, const std::string& name ) : _id(id), _name(name) {} \
  virtual ~CLASSNAME() {}                                      \
                                            \
  virtual void get_deps(                                       \
    std::vector< std::shared_ptr< abstract_schema > >& deps   \
    ) override;                                               \
  virtual void get_name( std::string& name ) override          \
  { name = _name; }                                            \
  virtual void get_str_schema( std::string& s ) override;      \
  virtual int64_t get_id() override                            \
  { return _id; }                                              \
                                            \
  std::string str_schema;                                      \
  int64_t _id = -1;                                            \
  std::string _name;

// Non-template version puts method definitions in separate macro
//    to avoid linker errors
#define HIVE_SCHEMA_CLASS_BODY( CLASSNAME )                 \
  CLASSNAME( int64_t id, const std::string& name );            \
  virtual ~CLASSNAME();                                        \
                                            \
  virtual void get_deps(                                       \
    std::vector< std::shared_ptr< abstract_schema > >& deps   \
    ) override;                                               \
  virtual void get_name( std::string& name ) override;         \
  virtual void get_str_schema( std::string& s ) override;      \
  virtual int64_t get_id() override;                           \
                                            \
  std::string str_schema;                                      \
  int64_t _id = -1;                                            \
  std::string _name;

// Non-template classes have to call this macro in a .cpp file
#define HIVE_SCHEMA_DEFINE_CLASS_METHODS( CLASSNAME )       \
  CLASSNAME::CLASSNAME( int64_t id, const std::string& name )  \
    : _id(id), _name(name) {}                                 \
  CLASSNAME::~CLASSNAME() {}                                   \
  void CLASSNAME::get_name( std::string& name )                \
  { name = _name; }                                            \
  int64_t CLASSNAME::get_id()                                  \
  { return _id; }

HIVE_DECLARE_SCHEMA_CLASS( false, false )
HIVE_DECLARE_SCHEMA_CLASS(  true, false )
HIVE_DECLARE_SCHEMA_CLASS(  true,  true )

template< typename ObjectType >
void schema_impl< ObjectType, false, false >::get_deps( std::vector< std::shared_ptr< abstract_schema > >& deps )
{ }

template< typename ObjectType >
void schema_impl< ObjectType,  true,  true >::get_deps( std::vector< std::shared_ptr< abstract_schema > >& deps )
{ }

template< typename ObjectType >
void schema_impl< ObjectType,  true, false >::get_deps( std::vector< std::shared_ptr< abstract_schema > >& deps )
{
  detail::get_deps_member_visitor vtor(deps);
  fc::reflector<ObjectType>::visit( vtor );
}

template< typename ObjectType >
void schema_impl< ObjectType, false, false >::get_str_schema( std::string& s )
{
  if( str_schema != "" )
  {
    s = str_schema;
    return;
  }

  std::string my_name;
  get_name( my_name );
  fc::variant_object_builder mvo = fc::variant_object_builder
    ("name", my_name)
    ("type", "prim");

  str_schema = fc::json::to_string( mvo.get() );
  s = str_schema;
  return;
}

template< typename ObjectType >
void schema_impl< ObjectType,  true, false >::get_str_schema( std::string& s )
{
  if( str_schema != "" )
  {
    s = str_schema;
    return;
  }

  detail::get_str_schema_class_member_visitor vtor;
  fc::reflector<ObjectType>::visit( vtor );

  std::string my_name;
  get_name( my_name );
  fc::variant_object_builder mvo = fc::variant_object_builder
    ("name", my_name)
    ("type", "class")
    ("members", vtor._members);

  str_schema = fc::json::to_string( mvo.get() );
  s = str_schema;
  return;
}

template< typename ObjectType >
void schema_impl< ObjectType,  true,  true >::get_str_schema( std::string& s )
{
  if( str_schema != "" )
  {
    s = str_schema;
    return;
  }

  detail::get_str_schema_enum_member_visitor vtor;
  fc::reflector<ObjectType>::visit( vtor );

  std::string my_name;
  get_name(my_name);
  fc::variant_object_builder mvo = fc::variant_object_builder
    ("name", my_name)
    ("type", "enum")
    ("members", vtor._members)
    ;

  str_schema = fc::json::to_string( mvo.get() );
  s = str_schema;
  return;
}

template<typename ObjectType >
std::shared_ptr< abstract_schema > create_schema( int64_t id )
{
  return std::make_shared< typename schema_reflect<ObjectType>::schema_impl_type >( id, fc::get_typename<ObjectType>::name() );
}

} } }

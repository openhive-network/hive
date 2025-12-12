// chainbase.inl - Template method implementations for chainbase::database
// Include this file only in .cpp files where explicit template instantiation is needed

#pragma once

#include <chainbase/chainbase.hpp>

namespace chainbase {

// Note: has_index() is kept inline in chainbase.hpp since it's called by inline get_index<MultiIndexType, ByIndex>()

template<typename MultiIndexType>
const generic_index<MultiIndexType>& database::get_index()const
{
  CHAINBASE_REQUIRE_READ_LOCK("get_index", typename MultiIndexType::value_type);
  typedef generic_index<MultiIndexType> index_type;
  typedef index_type*                   index_type_ptr;

  if( !has_index< MultiIndexType >() )
  {
    std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );
    CHAINBASE_THROW_EXCEPTION( std::runtime_error( "unable to find index for " + type_name + " in database" ) );
  }

  return *index_type_ptr( _index_map[index_type::value_type::type_id]->get() );
}

// Note: Two-parameter get_index<MultiIndexType, ByIndex>() is kept inline in chainbase.hpp
// because there are too many ByIndex tag combinations to explicitly instantiate

template<typename MultiIndexType>
generic_index<MultiIndexType>& database::get_mutable_index()
{
  CHAINBASE_REQUIRE_WRITE_LOCK("get_mutable_index", typename MultiIndexType::value_type);
  typedef generic_index<MultiIndexType> index_type;
  typedef index_type*                   index_type_ptr;

  if( !has_index< MultiIndexType >() )
  {
    std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );
    CHAINBASE_THROW_EXCEPTION( std::runtime_error( "unable to find index for " + type_name + " in database" ) );
  }

  return *index_type_ptr( _index_map[index_type::value_type::type_id]->get() );
}

} // namespace chainbase

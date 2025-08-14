#pragma once

#include <chainbase/chainbase.hpp>

namespace hive { namespace chain {

struct allocator_helper
{
  template<typename SHM_Object_Type, typename SHM_Object_Index>
  static auto get_allocator( const chainbase::database& db )
  {
    auto& _indices = db.get_index<SHM_Object_Index>().indices();
    auto _allocator = _indices.get_allocator();
    return chainbase::get_allocator_helper_t<SHM_Object_Type>::get_generic_allocator( _allocator );
  }

};

}}
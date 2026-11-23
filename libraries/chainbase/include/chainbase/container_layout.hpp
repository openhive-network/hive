#pragma once

#include <boost/core/demangle.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/throw_exception.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <typeinfo>

namespace chainbase {

  /**
    * Layout of a container stored in shared memory, as the build that constructed it lays it out.
    *
    * Containers in shared memory are found by name, and that name does not change when the container type or one of
    * its stateful members (allocators, hashers, comparators) does. The layout is kept in a separate named object, so
    * it can be read even when the container itself is laid out differently than the running build expects.
    */
  class container_layout
  {
    public:
      template< typename Container >
      static container_layout of()
      {
        return container_layout( sizeof( Container ), type_name_hash( boost::core::demangle( typeid( Container ).name() ) ) );
      }

      friend bool operator==( const container_layout& a, const container_layout& b )
      {
        return a._size == b._size && a._type_name_hash == b._type_name_hash;
      }

      std::string to_string() const
      {
        return "{size: " + std::to_string( _size ) + ", type hash: " + std::to_string( _type_name_hash ) + "}";
      }

    private:
      container_layout( uint64_t size, uint64_t type_name_hash ) : _size( size ), _type_name_hash( type_name_hash ) {}

      // FNV-1a: has to give the same value in every build, unlike std::hash
      static uint64_t type_name_hash( const std::string& name )
      {
        uint64_t hash = 0xcbf29ce484222325ULL;
        for( const unsigned char c : name )
        {
          hash ^= c;
          hash *= 0x100000001b3ULL;
        }
        return hash;
      }

      uint64_t _size;
      uint64_t _type_name_hash;
  };

  namespace detail {
    inline std::string container_layout_name( const std::string& container_name )
    {
      return container_name + "::layout";
    }
  }

  /// Stores the layout of Container for the container just constructed under container_name.
  template< typename Container >
  void record_container_layout( boost::interprocess::managed_mapped_file::segment_manager& segment_manager, const std::string& container_name )
  {
    const container_layout layout = container_layout::of< Container >();
    *segment_manager.find_or_construct< container_layout >( detail::container_layout_name( container_name ).c_str() )( layout ) = layout;
  }

  /**
    * Throws std::logic_error asking for a replay when the container found under container_name was laid out by a build
    * whose layout of Container differs from the running one, or by a build that did not record its layout.
    */
  template< typename Container >
  void verify_container_layout( boost::interprocess::managed_mapped_file::segment_manager& segment_manager, const std::string& container_name )
  {
    const container_layout* stored = segment_manager.find< container_layout >( detail::container_layout_name( container_name ).c_str() ).first;
    if( stored == nullptr )
      BOOST_THROW_EXCEPTION( std::logic_error( "The layout of " + container_name + " is not recorded in `shared_memory_file`: it was "
        "created by an older build. A replay (or a snapshot load) is needed." ) );
    const container_layout running = container_layout::of< Container >();
    if( !( *stored == running ) )
      BOOST_THROW_EXCEPTION( std::logic_error( "The layout of " + container_name + " in `shared_memory_file` " + stored->to_string() +
        " differs from the one of the running build " + running.to_string() + ". A replay (or a snapshot load) is needed." ) );
  }

} // namespace chainbase

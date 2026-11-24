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
      /// The layout of an object of type Container, together with the types of its stateful members (Members) when
      /// the type name of Container does not spell them out.
      template< typename Container, typename... Members >
      static container_layout of()
      {
        std::string type_names = boost::core::demangle( typeid( Container ).name() );
        ( ( type_names += ", " + boost::core::demangle( typeid( Members ).name() ) ), ... );
        return container_layout( ( sizeof( Container ) + ... + sizeof( Members ) ), type_name_hash( type_names ) );
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

  /// Thrown when a container in shared memory was laid out by a build other than the running one.
  class container_layout_mismatch : public std::logic_error
  {
    public:
      using std::logic_error::logic_error;
  };

  namespace detail {
    inline std::string container_layout_name( const std::string& container_name )
    {
      return container_name + "::layout";
    }

    /// Looks the object up without its type: a typed find asserts that the stored bytes fit that type.
    inline bool is_named_object_stored( boost::interprocess::managed_mapped_file::segment_manager& segment_manager, const std::string& name )
    {
      for( auto it = segment_manager.named_begin(); it != segment_manager.named_end(); ++it )
      {
        if( name.compare( 0, std::string::npos, it->name(), it->name_length() ) == 0 )
          return true;
      }
      return false;
    }
  }

  /// Stores the layout of Container (see container_layout::of) for the container constructed under container_name.
  /// Called before the container is constructed, so that no container is ever stored without its layout.
  template< typename Container, typename... Members >
  void record_container_layout( boost::interprocess::managed_mapped_file::segment_manager& segment_manager, const std::string& container_name )
  {
    const container_layout layout = container_layout::of< Container, Members... >();
    *segment_manager.find_or_construct< container_layout >( detail::container_layout_name( container_name ).c_str() )( layout ) = layout;
  }

  /**
    * Checks the layout recorded for the container stored under container_name, without looking up the container itself.
    * Returns true when the recorded layout is the one of the running build, so the container can be looked up as
    * Container, and false when neither the container nor its layout is stored.
    * Throws container_layout_mismatch asking for a replay when the recorded layout differs from the running one, or
    * when the container is stored without a layout, as a build that did not record layouts leaves it.
    */
  template< typename Container, typename... Members >
  bool verify_container_layout( boost::interprocess::managed_mapped_file::segment_manager& segment_manager, const std::string& container_name )
  {
    const container_layout* stored = segment_manager.find< container_layout >( detail::container_layout_name( container_name ).c_str() ).first;
    if( stored == nullptr )
    {
      if( !detail::is_named_object_stored( segment_manager, container_name ) )
        return false;
      BOOST_THROW_EXCEPTION( container_layout_mismatch( "The layout of " + container_name + " is not recorded in `shared_memory_file`: it was "
        "created by a build that does not record container layouts. A replay (or a snapshot load) is needed." ) );
    }
    const container_layout running = container_layout::of< Container, Members... >();
    if( !( *stored == running ) )
      BOOST_THROW_EXCEPTION( container_layout_mismatch( "The layout of " + container_name + " in `shared_memory_file` " + stored->to_string() +
        " differs from the one of the running build " + running.to_string() + " (the container type or the Boost version changed). "
        "A replay (or a snapshot load) is needed." ) );
    return true;
  }

} // namespace chainbase

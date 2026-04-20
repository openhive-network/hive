#pragma once

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/containers/flat_set.hpp>
#include <boost/interprocess/containers/flat_map.hpp>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/set.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include <string>
#include <vector>
#include <deque>
#include <set>
#include <map>
#include <type_traits>

// ENABLE_STD_ALLOCATOR is controlled via CMake option (cmake -DENABLE_STD_ALLOCATOR=ON)
// It is auto-enabled for sanitizer builds (CMAKE_BUILD_TYPE=Asan)

#define ENABLE_MULTI_INDEX_POOL_ALLOCATOR
#define ENABLE_UNDO_STATE_POOL_ALLOCATOR

namespace helpers {

namespace type_traits {

  template <typename, typename = std::void_t<>>
  struct has_value_type_member : std::false_type {};

  template <typename T>
  struct has_value_type_member<T, std::void_t<typename T::value_type>> : std::true_type {};

  template <typename T>
  inline constexpr bool has_value_type_member_v = has_value_type_member<T>::value;

} // namespace type_traits

template <typename T>
struct get_allocator_helper_t
{
  template <template <typename, uint32_t, bool> class Allocator,
            typename T2, uint32_t BLOCK_SIZE, bool USE_MANAGED_MAPPED_FILE>
  static auto get_generic_allocator(const Allocator<T2, BLOCK_SIZE, USE_MANAGED_MAPPED_FILE>& a)
  {
    return a.template get_generic_allocator<T>();
  }

  template <template <typename, typename> class Allocator, typename T2, typename TSegmentManager>
  static auto get_generic_allocator(const Allocator<T2, TSegmentManager>& a)
  {
    return Allocator<T, TSegmentManager>(a.get_segment_manager());
  }

  template <template <typename, typename, size_t> class Allocator, typename T2, typename TSegmentManager, size_t BLOCK_SIZE>
  static auto get_generic_allocator( const Allocator<T2, TSegmentManager, BLOCK_SIZE>& a )
  {
    return boost::interprocess::allocator<T, TSegmentManager>( a.get_segment_manager() );
  }

  template <template <typename> class Allocator, typename T2>
  static auto get_generic_allocator(const Allocator<T2>& a)
  {
    return Allocator<T>();
  }
};

} // namespace helpers

namespace chainbase {

  namespace bip = boost::interprocess;

#if defined(ENABLE_STD_ALLOCATOR)
  template <typename T>
  using allocator = std::allocator<T>;
#else
  template <typename T>
  using allocator = bip::allocator<T, bip::managed_mapped_file::segment_manager>;
#endif // ENABLE_STD_ALLOCATOR

#if defined(ENABLE_STD_ALLOCATOR)
  using shared_string = std::string;
#else
  using shared_string = bip::basic_string< char, std::char_traits< char >, allocator< char > >;
#endif // ENABLE_STD_ALLOCATOR

  template<typename T>
#if defined(ENABLE_STD_ALLOCATOR)
  using t_vector = std::vector< T, allocator<T> >;
#else
  using t_vector = bip::vector< T, allocator<T> >;
#endif // ENABLE_STD_ALLOCATOR

  template< typename FIRST_TYPE, typename SECOND_TYPE >
  using t_pair = std::pair< FIRST_TYPE, SECOND_TYPE >;

  template< typename FIRST_TYPE, typename SECOND_TYPE >
  using t_allocator_pair = allocator< t_pair< const FIRST_TYPE, SECOND_TYPE > >;

  template< typename KEY_TYPE, typename LESS_FUNC = std::less<KEY_TYPE>>
#if defined(ENABLE_STD_ALLOCATOR)
  using t_flat_set = boost::container::flat_set< KEY_TYPE, LESS_FUNC, allocator< KEY_TYPE > >;
#else
  using t_flat_set = bip::flat_set< KEY_TYPE, LESS_FUNC, allocator< KEY_TYPE > >;
#endif // ENABLE_STD_ALLOCATOR

  template< typename KEY_TYPE, typename LESS_FUNC = std::less<KEY_TYPE>, typename ALLOC = allocator< KEY_TYPE > >
#if defined(ENABLE_STD_ALLOCATOR)
  using t_set = std::set< KEY_TYPE, LESS_FUNC, ALLOC >;
#else
  using t_set = bip::set< KEY_TYPE, LESS_FUNC, ALLOC >;
#endif // ENABLE_STD_ALLOCATOR

  template< typename KEY_TYPE, typename VALUE_TYPE, typename LESS_FUNC = std::less<KEY_TYPE>>
#if defined(ENABLE_STD_ALLOCATOR)
  using t_flat_map = boost::container::flat_map< KEY_TYPE, VALUE_TYPE, LESS_FUNC, allocator< t_pair< KEY_TYPE, VALUE_TYPE > > >;
#else
  using t_flat_map = bip::flat_map< KEY_TYPE, VALUE_TYPE, LESS_FUNC, allocator< t_pair< KEY_TYPE, VALUE_TYPE > > >;
#endif // ENABLE_STD_ALLOCATOR

  template< typename KEY_TYPE, typename VALUE_TYPE, typename LESS_FUNC = std::less<KEY_TYPE>, typename ALLOC = allocator< t_pair< KEY_TYPE, VALUE_TYPE > > >
#if defined(ENABLE_STD_ALLOCATOR)
  using t_map = std::map< KEY_TYPE, VALUE_TYPE, LESS_FUNC, ALLOC >;
#else
  using t_map = bip::map< KEY_TYPE, VALUE_TYPE, LESS_FUNC, ALLOC >;
#endif // ENABLE_STD_ALLOCATOR

  template< typename T, typename ALLOC = allocator< T > >
#if defined(ENABLE_STD_ALLOCATOR)
  using t_deque = std::deque< T, ALLOC >;
#else
  using t_deque = bip::deque< T, ALLOC >;
#endif // ENABLE_STD_ALLOCATOR

} // namespace chainbase

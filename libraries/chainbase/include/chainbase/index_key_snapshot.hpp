#pragma once

#include <boost/mpl/size.hpp>
#include <boost/tuple/tuple.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace chainbase { namespace index_keys {

  template< typename T, typename = void >
  struct is_composite_key_extractor : std::false_type {};
  template< typename T >
  struct is_composite_key_extractor< T, std::void_t< typename T::key_extractor_tuple > > : std::true_type {};

  template< typename T, typename = void >
  struct owns_allocated_memory : std::false_type {};
  template< typename T >
  struct owns_allocated_memory< T, std::void_t< typename T::allocator_type > > : std::true_type {};

  template< typename T, typename = void >
  struct is_equality_comparable : std::false_type {};
  template< typename T >
  struct is_equality_comparable< T,
    std::void_t< decltype( bool( std::declval< const T& >() == std::declval< const T& >() ) ) > > : std::true_type {};

  // a copy of a key that owns allocator-backed memory would allocate (in shared memory for chainbase keys)
  template< typename T >
  constexpr bool is_snapshot_key_type = std::is_copy_constructible_v< T > && !owns_allocated_memory< T >::value &&
    is_equality_comparable< T >::value;

  template< typename Value, typename Cons >
  struct key_extractor_list_snapshot;

  /// Copy of the key one extractor takes from an element; composite keys become a std::tuple of their member keys.
  template< typename Value, typename KeyFromValue, bool Composite = is_composite_key_extractor< KeyFromValue >::value >
  struct key_snapshot
  {
    using type = std::decay_t< decltype( std::declval< const KeyFromValue& >()( std::declval< const Value& >() ) ) >;
    static constexpr bool supported = is_snapshot_key_type< type >;

    static type take( const KeyFromValue& extractor, const Value& v ) { return extractor( v ); }
  };

  template< typename Value, typename KeyFromValue >
  struct key_snapshot< Value, KeyFromValue, true >
  {
    using members = key_extractor_list_snapshot< Value, typename KeyFromValue::key_extractor_tuple::inherited >;
    using type = typename members::type;
    static constexpr bool supported = members::supported;

    static type take( const KeyFromValue& extractor, const Value& v ) { return members::take( extractor.key_extractors(), v ); }
  };

  /// Walks the boost::tuples::cons list of a composite_key's extractors.
  template< typename Value, typename Cons >
  struct key_extractor_list_snapshot
  {
    using head = key_snapshot< Value, typename Cons::head_type >;
    using tail = key_extractor_list_snapshot< Value, typename Cons::tail_type >;
    using type = decltype( std::tuple_cat( std::declval< std::tuple< typename head::type > >(), std::declval< typename tail::type >() ) );
    static constexpr bool supported = head::supported && tail::supported;

    static type take( const Cons& c, const Value& v )
    {
      return std::tuple_cat( std::tuple< typename head::type >( head::take( c.get_head(), v ) ), tail::take( c.get_tail(), v ) );
    }
  };

  template< typename Value >
  struct key_extractor_list_snapshot< Value, boost::tuples::null_type >
  {
    using type = std::tuple<>;
    static constexpr bool supported = true;

    static type take( const boost::tuples::null_type&, const Value& ) { return {}; }
  };

  /**
    * Copies, by value, of the keys every index of a multi_index_container takes from one element, so they can be
    * compared after the element was changed in place: if no key changed, every index still holds the element
    * in a valid position and the container needs no re-indexing.
    *
    * `supported` is false when some index key cannot be copied that way (it owns allocator-backed memory or has no
    * operator==); such a container must always be re-indexed.
    *
    * Keys are compared with operator==, which is correct as long as a == b implies the index treats a and b as
    * equivalent (same position for ordered indexes, same hash and equality for hashed ones).
    */
  template< typename Value, typename MultiIndexType,
    typename Indexes = std::make_index_sequence< boost::mpl::size< typename MultiIndexType::index_type_list >::value > >
  class index_key_snapshot;

  template< typename Value, typename MultiIndexType, std::size_t... I >
  class index_key_snapshot< Value, MultiIndexType, std::index_sequence< I... > >
  {
    template< std::size_t N >
    using nth_key = key_snapshot< Value, typename MultiIndexType::template nth_index< N >::type::key_from_value >;

  public:
    static constexpr bool supported = ( nth_key< I >::supported && ... );

    using type = std::tuple< typename nth_key< I >::type... >;

    static type take( const MultiIndexType& indices, const Value& v )
    {
      return type( nth_key< I >::take( indices.template get< I >().key_extractor(), v )... );
    }
  };

} } // namespace chainbase::index_keys

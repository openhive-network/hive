#pragma once

#include <boost/mpl/size.hpp>

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace hive { namespace chain {

  namespace index_consistency_detail
  {
    template< typename Index, typename = void >
    struct is_ordered_index : std::false_type {};
    template< typename Index >
    struct is_ordered_index< Index, std::void_t< typename Index::key_compare > > : std::true_type {};

    template< typename Index >
    bool index_finds_by_current_key( const Index& idx, const typename Index::value_type& obj )
    {
      const auto range = idx.equal_range( idx.key_extractor()( obj ) );
      for( auto it = range.first; it != range.second; ++it )
        if( &*it == &obj )
          return true;
      return false;
    }

    template< typename Index >
    bool index_in_key_order( const Index& idx )
    {
      if constexpr( is_ordered_index< Index >::value )
      {
        const auto& key = idx.key_extractor();
        const auto& comp = idx.key_comp();
        for( auto it = idx.begin(), next = std::next( it ); it != idx.end() && next != idx.end(); ++it, ++next )
          if( comp( key( *next ), key( *it ) ) )
            return false;
      }
      return true;
    }

    template< typename Index >
    bool index_keys_equivalent( const Index& idx, const typename Index::value_type& a, const typename Index::value_type& b )
    {
      const auto& key = idx.key_extractor();
      if constexpr( is_ordered_index< Index >::value )
        return !idx.key_comp()( key( a ), key( b ) ) && !idx.key_comp()( key( b ), key( a ) );
      else
        return idx.key_eq()( key( a ), key( b ) );
    }

    template< typename MultiIndex, std::size_t... I >
    bool same_index_keys( const MultiIndex& container, const typename MultiIndex::value_type& a,
      const typename MultiIndex::value_type& b, std::index_sequence< I... > )
    {
      return ( index_keys_equivalent( container.template get< I >(), a, b ) && ... );
    }

    template< typename MultiIndex, std::size_t... I >
    bool every_index_finds( const MultiIndex& container, const typename MultiIndex::value_type& obj, std::index_sequence< I... > )
    {
      return ( index_finds_by_current_key( container.template get< I >(), obj ) && ... );
    }

    template< typename MultiIndex, std::size_t... I >
    bool every_index_ordered( const MultiIndex& container, std::index_sequence< I... > )
    {
      return ( index_in_key_order( container.template get< I >() ) && ... );
    }

    template< typename MultiIndex >
    using index_numbers = std::make_index_sequence< boost::mpl::size< typename MultiIndex::index_type_list >::value >;
  }

  /// True when every index of the container finds the object by the key it takes from the object now.
  template< typename MultiIndex >
  bool every_index_finds( const MultiIndex& container, const typename MultiIndex::value_type& obj )
  {
    return index_consistency_detail::every_index_finds( container, obj, index_consistency_detail::index_numbers< MultiIndex >() );
  }

  /// True when every index of the container takes equivalent keys from both objects (which may live in different containers).
  template< typename MultiIndex >
  bool same_index_keys( const MultiIndex& container, const typename MultiIndex::value_type& a, const typename MultiIndex::value_type& b )
  {
    return index_consistency_detail::same_index_keys( container, a, b, index_consistency_detail::index_numbers< MultiIndex >() );
  }

  /**
    * True when every index of the container holds every element: each one finds every element by its current key
    * and each ordered index iterates in key order.
    */
  template< typename MultiIndex >
  bool indexes_are_consistent( const MultiIndex& container )
  {
    for( const auto& obj : container )
      if( !every_index_finds( container, obj ) )
        return false;
    return index_consistency_detail::every_index_ordered( container, index_consistency_detail::index_numbers< MultiIndex >() );
  }

} }

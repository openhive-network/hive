#pragma once
#include <boost/variant.hpp>

#include <memory>

namespace mira {

namespace multi_index {
namespace detail {
template< typename Value, typename Key, typename KeyFromValue,
   typename KeyCompare, typename ID, typename IDFromValue >
   struct rocksdb_iterator;
} }

template< typename ValueType, typename... Iters >
class iterator_adapter :
   public boost::bidirectional_iterator_helper<
      iterator_adapter< ValueType, Iters... >,
      ValueType,
      std::size_t,
      const ValueType*,
      const ValueType& >
{
   typedef boost::variant< Iters... > iter_variant;

   template< typename t > struct type {};

   public:
      mutable iter_variant _itr;

      iterator_adapter() = default;
      iterator_adapter( const iterator_adapter& rhs ) = default;
      iterator_adapter( iterator_adapter&& rhs ) = default;

      template< typename T >
      static iterator_adapter create( const T& rhs )
      {
         iterator_adapter itr;
         itr._itr = rhs;
         return itr;
      }

      template< typename T >
      static iterator_adapter create( T&& rhs )
      {
         iterator_adapter itr;
         itr._itr = std::move( rhs );
         return itr;
      }

      bool valid() const
      {
         return boost::apply_visitor(
            [this]( auto& itr ) { return validate_iterator(itr); },
            _itr
         );
      }

      template< typename Value, typename Key, typename KeyFromValue,
         typename KeyCompare, typename ID, typename IDFromValue >
      bool validate_iterator( const multi_index::detail::rocksdb_iterator<Value, Key, KeyFromValue, KeyCompare, ID, IDFromValue>& i ) const
      {
         return i.valid();
      }

      template <class T>
      bool validate_iterator( const T& i ) const
      {
         return true;
      }

      iterator_adapter& operator ++()
      {
         boost::apply_visitor(
            []( auto& itr ){ ++itr; },
            _itr
         );

         return *this;
      }

      iterator_adapter operator ++(int)const
      {
         return boost::apply_visitor(
            [this]( auto& itr )
            {
               iterator_adapter copy( *this );
               itr++;
               return copy;
            },
            _itr
         );
      }

      iterator_adapter& operator --()
      {
         boost::apply_visitor(
            []( auto& itr ){ --itr; },
            _itr
         );

         return *this;
      }

      iterator_adapter operator --(int)const
      {
         return boost::apply_visitor(
            [this]( auto& itr )
            {
               iterator_adapter copy( *this );
               itr--;
               return copy;
            },
            _itr
         );
      }

      const ValueType& operator *() const
      {
         return *(operator ->());
      }

      const ValueType* operator ->() const
      {
         return boost::apply_visitor(
            []( auto& itr ){ return itr.operator->(); },
            _itr
         );
      }

      iterator_adapter& operator =( const iterator_adapter& rhs ) = default;
      iterator_adapter& operator =( iterator_adapter&& rhs ) = default;

      template< typename IterType >
      IterType& get() { return boost::get< IterType >( _itr ); }
};

template< typename ValueType, typename... Iters >
bool operator ==( const iterator_adapter< ValueType, Iters... >& lhs, const iterator_adapter< ValueType, Iters... >& rhs )
{
   return lhs._itr == rhs._itr;
}

template< typename ValueType, typename... Iters >
bool operator !=( const iterator_adapter< ValueType, Iters... >& lhs, const iterator_adapter< ValueType, Iters... >& rhs )
{
   return !( lhs == rhs );
}

} // mira

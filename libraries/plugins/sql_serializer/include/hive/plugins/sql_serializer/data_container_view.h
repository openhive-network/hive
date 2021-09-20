#pragma once

#include <iterator>
#include <memory>

namespace hive::plugins::sql_serializer {

  template< typename Container >
  class container_view {
  public:
    using const_reference = typename Container::const_reference;
    using container = Container;

    container_view( std::shared_ptr< Container > container, typename Container::const_iterator begin, typename Container::const_iterator end );
    container_view( container_view&& _rh );
    container_view( container_view& ) = delete;
    container_view() = delete;
    container_view& operator=( container_view&& _rh ) = delete;
    container_view& operator=( container_view& _rh ) = delete;


    typename Container::const_iterator cbegin() const { return _begin; }
    typename Container::const_iterator cend() const { return _end; }
    bool empty() const { return _begin == _end; }
    std::size_t size() const { return std::distance( _begin, _end ); }
  private:
    std::shared_ptr< Container > _container;
    typename Container::const_iterator _begin;
    typename Container::const_iterator _end;
  };

  template< typename Container >
  inline
  container_view< Container >::container_view( std::shared_ptr< Container > container, typename Container::const_iterator begin, typename Container::const_iterator end )
    : _container( container )
    , _begin( begin )
    , _end( end ) {
  }

  template< typename Container >
  inline
  container_view< Container >::container_view( container_view&& _rh )
  :   _container( std::move( _rh._container ) )
    , _begin( _rh._begin )
    , _end( _rh._end ) {
    _rh._begin = _rh._end;
  }


} // namespace hive::plugins::sql_serializer

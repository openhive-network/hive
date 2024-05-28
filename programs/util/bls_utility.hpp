#pragma once

#include "bls_pop_scheme.hpp"
#include "bls_aug_scheme.hpp"
#include "bls_m_of_n_scheme.hpp"

#include <thread>
#include <future>
#include <list>

using namespace bls;

void sum( std::promise<G1Element>&& p, const std::vector<G1Element>& public_keys, uint32_t begin, uint32_t end )
{
  G1Element _result;

  for( size_t i = begin; i < end; ++i )
    _result += public_keys[i];

  p.set_value( _result );
}

template< typename T >
bool verify( T& obj, const std::vector<G1Element>& public_keys, uint32_t nr_threads )
{
  if( nr_threads == 0 )
    return false;

  if( nr_threads == 1 )
  {
    G1Element _pk;

    auto _start = std::chrono::high_resolution_clock::now();

    // for( auto& item : public_keys )
    //   _pk += item;

    auto _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"SUM: "<<" time: "<<_interval<<"[us]"<<std::endl;

    _start = std::chrono::high_resolution_clock::now();
    //bool _result = obj.verify( { _pk } );
    bool _result = obj.verify( public_keys );
    _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"CAL: time: "<<_interval<<"[us]"<<std::endl;

    return _result;
  }
  else
  {
    auto _start = std::chrono::high_resolution_clock::now();
    auto _size = public_keys.size();
    auto _range = _size / nr_threads;

    std::list<std::thread> threads;

    using _promise_type = std::promise<G1Element>;
    using _future_type = std::future<G1Element>;

    std::vector<_promise_type> promises( nr_threads );
    std::list<_future_type> futures;

    for( uint32_t i = 0; i < nr_threads; ++i )
    {
      auto _begin = i * _range;
      auto _end =  ( i == nr_threads - 1 ) ? _size : ( (i + 1) * _range );

      futures.emplace_back( promises[i].get_future() );
      threads.emplace_back( std::thread( sum, std::move( promises[i] ), public_keys, _begin, _end ) );
    }

    for( auto& thread : threads )
      thread.join();

    std::vector<G1Element> _public_keys;

    for( auto& future : futures )
    {
      _public_keys.emplace_back( future.get() );
    }
    auto _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"SUM: "<<" time: "<<_interval<<"[us]"<<std::endl;

    _start = std::chrono::high_resolution_clock::now();
    bool _result = obj.verify( _public_keys );
    _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"CAL: time: "<<_interval<<"[us]"<<std::endl;

    return _result;
  }
}
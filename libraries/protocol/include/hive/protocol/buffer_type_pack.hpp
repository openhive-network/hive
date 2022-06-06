#pragma once

#include <fc/io/datastream.hpp>
#include <fc/io/raw.hpp>

namespace fc { namespace raw {

template< typename T, typename B > inline void pack_to_buffer( B& raw, const T& v )
{
  auto size = pack_size( v );
  raw.resize( size );
  datastream< char* > ds( raw.data(), size );
  pack( ds, v );
}

template< typename T, typename B > inline void unpack_from_buffer( const B& raw, T& v )
{
  datastream< const char* > ds( raw.data(), raw.size() );
  unpack( ds, v );
}

template< typename T, typename B > inline T unpack_from_buffer( const B& raw )
{
  T v;
  datastream< const char* > ds( raw.data(), raw.size() );
  unpack( ds, v );
  return v;
}

} } // fc::raw

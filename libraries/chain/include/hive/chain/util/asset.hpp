#pragma once

#include <hive/protocol/asset.hpp>

namespace hive { namespace chain { namespace util {

using hive::protocol::asset;
using hive::protocol::price;

inline asset to_hbd( const price& p, const asset& hive )
{
  FC_ASSERT( hive.symbol == HIVE_SYMBOL );
  if( p.is_null() )
    return asset( 0, HBD_SYMBOL );
  return hive * p;
}

inline asset to_hive( const price& p, const asset& hbd )
{
  FC_ASSERT( hbd.symbol == HBD_SYMBOL );
  if( p.is_null() )
    return asset( 0, HIVE_SYMBOL );
  return hbd * p;
}

} } }

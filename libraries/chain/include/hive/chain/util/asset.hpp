#pragma once

#include <hive/protocol/asset.hpp>

namespace hive { namespace chain { namespace util {

using hive::protocol::asset;
using hive::protocol::price;
using hive::protocol::HBD_asset;
using hive::protocol::HIVE_asset;

inline HBD_asset to_hbd( const price& p, const asset& hive )
{
  FC_ASSERT( hive.symbol == HIVE_SYMBOL );
  if( p.is_null() )
    return HBD_asset( 0 );
  return hive * p;
}

inline HIVE_asset to_hive( const price& p, const asset& hbd )
{
  FC_ASSERT( hbd.symbol == HBD_SYMBOL );
  if( p.is_null() )
    return HIVE_asset( 0 );
  return hbd * p;
}

} } }
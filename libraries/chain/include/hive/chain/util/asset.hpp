#pragma once

#include <hive/protocol/asset.hpp>

namespace hive { namespace chain { namespace util {

using hive::protocol::HBD_asset;
using hive::protocol::HIVE_asset;
using hive::protocol::HBD_price;

inline HBD_asset to_hbd( const HBD_price& p, const HIVE_asset& hive )
{
  if( p.is_null() )
    return HBD_asset( 0 );
  return hive * p;
}

inline HIVE_asset to_hive( const HBD_price& p, const HBD_asset& hbd )
{
  if( p.is_null() )
    return HIVE_asset( 0 );
  return hbd * p;
}

} } }

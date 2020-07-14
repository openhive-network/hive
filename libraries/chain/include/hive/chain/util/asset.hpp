#pragma once

#include <hive/protocol/asset.hpp>

namespace hive { namespace chain { namespace util {

using hive::protocol::HBD_asset;
using hive::protocol::HIVE_asset;
using hive::protocol::price;

inline HBD_asset compute_hbd( const price& p, const HIVE_asset& hive )
{
  if( p.is_null() )
    return HBD_asset( 0 );
  return ( hive.to_asset() * p ).to_HBD();
}

inline HIVE_asset compute_hive( const price& p, const HBD_asset& hbd )
{
  if( p.is_null() )
    return HIVE_asset( 0 );
  return ( hbd.to_asset() * p ).to_HIVE();
}

} } }

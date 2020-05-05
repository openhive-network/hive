#pragma once

#include <hive/protocol/asset.hpp>

namespace hive { namespace chain { namespace util {

using hive::protocol::asset;
using hive::protocol::price;

inline asset to_hbd( const price& p, const asset& steem )
{
   FC_ASSERT( steem.symbol == HIVE_SYMBOL );
   if( p.is_null() )
      return asset( 0, HBD_SYMBOL );
   return steem * p;
}

inline asset to_steem( const price& p, const asset& hbd )
{
   FC_ASSERT( hbd.symbol == HBD_SYMBOL );
   if( p.is_null() )
      return asset( 0, HIVE_SYMBOL );
   return hbd * p;
}

} } }

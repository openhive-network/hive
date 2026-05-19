#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/protocol/hive_virtual_operations.hpp>

#ifndef HIVE_MARKET_HISTORY_SPACE_ID
#define HIVE_MARKET_HISTORY_SPACE_ID 7
#endif

namespace hive { namespace plugins { namespace market_history {

using namespace hive::chain;

using hive::protocol::price;
using hive::protocol::asset;

enum market_history_object_types
{
  bucket_object_type        = ( HIVE_MARKET_HISTORY_SPACE_ID << 8 ),
  order_history_object_type = ( HIVE_MARKET_HISTORY_SPACE_ID << 8 ) + 1
};

struct bucket_object_details
{
  share_type           high;
  share_type           low;
  share_type           open;
  share_type           close;
  share_type           volume;

  void fill( const share_type& val )
  {
    high = val;
    low = val;
    open = val;
    close = val;
    volume = val;
  }
};

struct bucket_object : public object< bucket_object_type, bucket_object >
{
  CHAINBASE_OBJECT( bucket_object, true );

public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( bucket_object )

  fc::time_point_sec   open;
  uint32_t             seconds = 0;

  bucket_object_details hive;
  bucket_object_details non_hive;

  price high()const { return price( asset( non_hive.high, HBD_SYMBOL ), asset( hive.high, HIVE_SYMBOL ) ); }
  price low()const { return price( asset( non_hive.low, HBD_SYMBOL ), asset( hive.low, HIVE_SYMBOL ) ); }
};

typedef oid_ref< bucket_object > bucket_id_type;


struct order_history_object : public object< order_history_object_type, order_history_object >
{
  CHAINBASE_OBJECT( order_history_object );

public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( order_history_object )

  fc::time_point_sec               time;
  protocol::fill_order_operation   op;
};

typedef oid_ref< order_history_object > order_history_id_type;

} } } // hive::plugins::market_history

FC_REFLECT( hive::plugins::market_history::bucket_object_details,
        (high)
        (low)
        (open)
        (close)
        (volume) )

FC_REFLECT( hive::plugins::market_history::bucket_object,
              (id)
              (open)(seconds)
              (hive)
              (non_hive)
      )

FC_REFLECT( hive::plugins::market_history::order_history_object,
              (id)
              (time)
              (op) )

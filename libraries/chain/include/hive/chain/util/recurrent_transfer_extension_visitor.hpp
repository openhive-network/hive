#pragma once

#include <hive/protocol/types.hpp>

#include <hive/chain/database.hpp>

namespace hive { namespace chain {

struct recurrent_transfer_extension_visitor
{
  recurrent_transfer_extension_visitor( database& db ) : _db( db ) {}

  database& _db;
  uint8_t pair_id = 0; // default recurrent transfer id is 0

  typedef void result_type;

  void operator()( const recurrent_transfer_pair_id& recurrent_transfer_pair_id )
  {
    FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_28 ), "recurrent_transfer_pair_id extension requires hardfork ${hf}",
      ( "hf", HIVE_HARDFORK_1_28 ) );
    pair_id = recurrent_transfer_pair_id.pair_id;
  }

  void operator()( const hive::void_t& ) {}
};

} } // hive::chain
#pragma once

#include <hive/protocol/types.hpp>

#include <hive/chain/database.hpp>

namespace hive { namespace chain {

struct recurrent_transfer_extension_visitor
{
  uint8_t pair_id = 0; // default recurrent transfer id is 0
  bool was_pair_id = false;

  typedef void result_type;

  void operator()( const recurrent_transfer_pair_id& recurrent_transfer_pair_id )
  {
    was_pair_id = true;
    pair_id = recurrent_transfer_pair_id.pair_id;
  }

  void operator()( const hive::void_t& ) {}
};

} } // hive::chain
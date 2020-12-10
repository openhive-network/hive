#pragma once

#include <hive/protocol/asset.hpp>

#include<string>

namespace hive { namespace chain {

using hive::protocol::asset;

struct hf23_item
{
  asset       balance;
  asset       hbd_balance;
};

} } // namespace hive::chain

FC_REFLECT( hive::chain::hf23_item,
  (balance)(hbd_balance) )

#pragma once

#include <hive/protocol/asset.hpp>

#include<string>

namespace hive { namespace chain {

using hive::protocol::HIVE_asset;
using hive::protocol::HBD_asset;

struct hf23_item
{
  HIVE_asset  hive_balance;
  HBD_asset   hbd_balance;
};

} } // namespace hive::chain

FC_REFLECT( hive::chain::hf23_item,
  (hive_balance)(hbd_balance) )

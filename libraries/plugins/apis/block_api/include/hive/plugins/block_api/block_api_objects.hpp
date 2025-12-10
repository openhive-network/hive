#pragma once
#include <hive/protocol/block.hpp>
#include <hive/protocol/types.hpp>

#include <memory>
#include <vector>

namespace hive { namespace chain {
  // Forward declarations
  class full_block_type;
  class full_transaction_type;
} }

namespace hive { namespace plugins { namespace block_api {

using hive::protocol::signed_block;
using hive::protocol::block_id_type;
using hive::protocol::public_key_type;
using hive::protocol::transaction_id_type;
using std::vector;

struct api_signed_block_object : public signed_block
{
  api_signed_block_object(const std::shared_ptr<hive::chain::full_block_type>& full_block);
  api_signed_block_object() {}

  block_id_type                 block_id;
  public_key_type               signing_key;
  vector< transaction_id_type > transaction_ids;
};

} } } // hive::plugins::database_api

FC_REFLECT_DERIVED( hive::plugins::block_api::api_signed_block_object, (hive::protocol::signed_block),
              (block_id)
              (signing_key)
              (transaction_ids)
            )

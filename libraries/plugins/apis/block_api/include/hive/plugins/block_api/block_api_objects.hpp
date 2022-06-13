#pragma once
#include <hive/chain/account_object.hpp>
#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/database.hpp>

namespace hive { namespace plugins { namespace block_api {

using namespace hive::chain;

struct api_signed_block_object : public signed_block
{
  api_signed_block_object(const std::shared_ptr<full_block_type>& full_block) : 
    signed_block(full_block->get_block()),
    block_id(full_block->get_block_id()),
    signing_key(full_block->get_signing_key())
  {
    const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions = full_block->get_full_transactions();
    transaction_ids.reserve(transactions.size());
    for (const std::shared_ptr<full_transaction_type>& full_transaction : full_transactions)
      transaction_ids.push_back(full_transaction->get_transaction_id());
  }
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

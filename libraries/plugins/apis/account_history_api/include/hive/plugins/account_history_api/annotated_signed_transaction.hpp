#pragma once

#include <hive/protocol/transaction.hpp>

namespace hive { namespace plugins { namespace account_history {

/// Type shared across several APIs: mostly used in wallet (related) APIs and AH. Once AH will finally disappear from hived project we can consider moving it into wallet_bridge_api.
struct annotated_signed_transaction : public hive::protocol::signed_transaction
{
  using transaction_id_type=hive::protocol::transaction_id_type;

  /// Required by serialization
  annotated_signed_transaction() = default;
  /// Required by wallet when standalone tx is instantiated 
  annotated_signed_transaction(const signed_transaction& trx, const transaction_id_type& tx_id) : signed_transaction(trx), transaction_id(tx_id) {}
  /// Required by AH API implementation
  annotated_signed_transaction(const signed_transaction& trx, transaction_id_type tx_id, uint32_t block, uint32_t tx_in_block, fc::optional<int64_t> cost) :
    signed_transaction(trx), transaction_id(tx_id), block_num(block), transaction_num(tx_in_block)
  {
    if( cost.valid() && *cost >= 0 )
      rc_cost = cost;
  }

  transaction_id_type     transaction_id;
  uint32_t                block_num = 0;
  uint32_t                transaction_num = 0;
  fc::optional< int64_t > rc_cost;
};

}}} // hive::plugins::account_history

FC_REFLECT_DERIVED(hive::plugins::account_history::annotated_signed_transaction, (hive::protocol::signed_transaction), (transaction_id)(block_num)(transaction_num)(rc_cost));


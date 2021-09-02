#pragma once

#include <hive/plugins/sql_serializer/tables_descriptions.h>

namespace hive::plugins::sql_serializer {
  struct cached_data_t
    {

    hive_blocks::container_t blocks;
    std::vector<PSQL::processing_objects::process_transaction_t> transactions;
    hive_transactions_multisig::container_t transactions_multisig;
    std::vector<PSQL::processing_objects::process_operation_t> operations;

    size_t total_size;

    explicit cached_data_t(const size_t reservation_size) : total_size{ 0ul }
    {
      blocks.reserve(reservation_size);
      transactions.reserve(reservation_size);
      transactions_multisig.reserve(reservation_size);
      operations.reserve(reservation_size);
    }

    ~cached_data_t()
    {
      ilog(
        "blocks: ${b} trx: ${t} operations: ${o} total size: ${ts}...",
        ("b", blocks.size() )
        ("t", transactions.size() )
        ("o", operations.size() )
        ("ts", total_size )
        );
    }

    };

  using cached_containter_t = std::unique_ptr<cached_data_t>;
} //namespace hive::plugins::sql_serializer
#include <hive/plugins/sql_serializer/tables_descriptions.h>

namespace hive::plugins::sql_serializer {

  const char hive_blocks::TABLE[] = "hive.blocks";
  const char hive_blocks::COLS[] = "num, hash, prev, created_at";

  const char hive_transactions::TABLE[] = "hive.transactions";
  const char hive_transactions::COLS[] = "block_num, trx_hash, trx_in_block, ref_block_num, ref_block_prefix, expiration, signature";

  const char hive_transactions_multisig::TABLE[] = "hive.transactions_multisig";
  const char hive_transactions_multisig::COLS[] = "trx_hash, signature";

  const char hive_operations::TABLE[] = "hive.operations";
  const char hive_operations::COLS[] = "id, block_num, trx_in_block, op_pos, op_type_id, body";
} // namespace hive::plugins::sql_serializer



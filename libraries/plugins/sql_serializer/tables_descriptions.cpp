#include <hive/plugins/sql_serializer/tables_descriptions.h>

namespace hive{ namespace plugins{ namespace sql_serializer {

  const char hive_blocks::TABLE[] = "hive.blocks";
  const char hive_blocks::COLS[] = "num, hash, prev, created_at";

  template<> const char hive_transactions< std::vector<PSQL::processing_objects::process_transaction_t> >::TABLE[] = "hive.transactions";
  template<> const char hive_transactions< std::vector<PSQL::processing_objects::process_transaction_t> >::COLS[] = "block_num, trx_in_block, trx_hash, ref_block_num, ref_block_prefix, expiration, signature";

  template<> const char hive_transactions< container_view< std::vector<PSQL::processing_objects::process_transaction_t> > >::TABLE[] = "hive.transactions";
  template<> const char hive_transactions< container_view< std::vector<PSQL::processing_objects::process_transaction_t> > >::COLS[] = "block_num, trx_in_block, trx_hash, ref_block_num, ref_block_prefix, expiration, signature";

  const char hive_transactions_multisig::TABLE[] = "hive.transactions_multisig";
  const char hive_transactions_multisig::COLS[] = "trx_hash, signature";

  template<> const char hive_operations< container_view< std::vector<PSQL::processing_objects::process_operation_t> > >::TABLE[] = "hive.operations";
  template<> const char hive_operations< container_view< std::vector<PSQL::processing_objects::process_operation_t> > >::COLS[] = "id, block_num, trx_in_block, op_pos, op_type_id, body";

  template<> const char  hive_operations< std::vector<PSQL::processing_objects::process_operation_t> >::TABLE[] = "hive.operations";
  template<> const char  hive_operations< std::vector<PSQL::processing_objects::process_operation_t> >::COLS[] = "id, block_num, trx_in_block, op_pos, op_type_id, body";
}}} // namespace hive::plugins::sql_serializer



#pragma once

#include <hive/plugins/sql_serializer/sql_serializer_objects.hpp>
#include <hive/plugins/sql_serializer/data_2_sql_tuple_base.h>

#include <fc/io/json.hpp>

namespace hive::plugins::sql_serializer {

  struct hive_blocks
    {
    using container_t = std::vector<PSQL::processing_objects::process_block_t>;

    static const char TABLE[];
    static const char COLS[];

    struct data2sql_tuple : public data2_sql_tuple_base
      {
      using data2_sql_tuple_base::data2_sql_tuple_base;

      std::string operator()(typename container_t::const_reference data) const
      {
        return std::to_string(data.block_number) + "," + escape_raw(data.hash) + "," +
        escape_raw(data.prev_hash) + ", '" + data.created_at.to_iso_string() + '\'';
      }
      };
    };

  struct hive_transactions
    {
    using container_t = std::vector<PSQL::processing_objects::process_transaction_t>;

    static const char TABLE[];
    static const char COLS[];

    struct data2sql_tuple : public data2_sql_tuple_base
      {
      using data2_sql_tuple_base::data2_sql_tuple_base;

      std::string operator()(typename container_t::const_reference data) const
      {
        return std::to_string(data.block_number) + "," + escape_raw(data.hash) + "," + std::to_string(data.trx_in_block) + "," +
        std::to_string(data.ref_block_num) + "," + std::to_string(data.ref_block_prefix) + ",'" + data.expiration.to_iso_string() + "'," + escape_raw(data.signature);
      }
      };
    };

  struct hive_transactions_multisig
    {
    using container_t = std::vector<PSQL::processing_objects::process_transaction_multisig_t>;

    static const char TABLE[];
    static const char COLS[];

    struct data2sql_tuple : public data2_sql_tuple_base
      {
      using data2_sql_tuple_base::data2_sql_tuple_base;

      std::string operator()(typename container_t::const_reference data) const
      {
        return escape_raw(data.hash) + "," + escape_raw(data.signature);
      }
      };
    };

  struct hive_operations
    {
    using container_t = std::vector<PSQL::processing_objects::process_operation_t>;

    static const char TABLE[];
    static const char COLS[];

    struct data2sql_tuple : public data2_sql_tuple_base
      {
      using data2_sql_tuple_base::data2_sql_tuple_base;

      std::string operator()(typename container_t::const_reference data) const
      {
        // deserialization
        fc::variant opVariant;
        fc::to_variant(data.op, opVariant);
        fc::string deserialized_op = fc::json::to_string(opVariant);

        return std::to_string(data.operation_id) + ',' + std::to_string(data.block_number) + ',' +
        std::to_string(data.trx_in_block) + ',' + std::to_string(data.op_in_trx) + ',' +
        std::to_string(data.op.which()) + ',' + escape(deserialized_op);
      }
      };
    };
} // namespace hive::plugins::sql_serializer
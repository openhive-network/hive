#pragma once

#include <hive/plugins/sql_serializer/data_dumper.h>

#include <hive/plugins/sql_serializer/table_data_writer.h>
#include <hive/plugins/sql_serializer/tables_descriptions.h>
#include <hive/plugins/sql_serializer/data_processor.hpp>

#include <hive/plugins/sql_serializer/cached_data.h>

#include <memory>
#include <string>

namespace hive::plugins::sql_serializer {
  class transaction_controller;

  class livesync_data_dumper : public data_dumper {
  public:
    livesync_data_dumper( std::string db_url );

    ~livesync_data_dumper() { join(); }
    livesync_data_dumper(livesync_data_dumper&) = delete;
    livesync_data_dumper(livesync_data_dumper&&) = delete;
    livesync_data_dumper& operator=(livesync_data_dumper&&) = delete;
    livesync_data_dumper& operator=(livesync_data_dumper&) = delete;

    void trigger_data_flush( cached_data_t& cached_data, int last_block_num ) override;
    void join() override;
    void wait_for_data_processing_finish() override;
    uint32_t blocks_per_flush() const override { return 1; }
  private:
    // [TODO] move to separated class
    std::string block;
    std::string transactions;
    std::string transactions_multisig;
    std::string operations;

    std::shared_ptr< transaction_controller > transactions_controller;
  private:
    using block_data_container_t_writer = table_data_writer<hive_blocks, string_data_processor>;
    using transaction_data_container_t_writer = table_data_writer<hive_transactions, string_data_processor>;
    using transaction_multisig_data_container_t_writer = table_data_writer<hive_transactions_multisig, string_data_processor>;
    using operation_data_container_t_writer = table_data_writer<hive_operations, string_data_processor>;

    std::unique_ptr< block_data_container_t_writer > _block_writer;
    std::unique_ptr< transaction_data_container_t_writer > _transaction_writer;
    std::unique_ptr< transaction_multisig_data_container_t_writer > _transaction_multisig_writer;
    std::unique_ptr< operation_data_container_t_writer > _operation_writer;
  };

} // namespace hive::plugins::sql_serializer

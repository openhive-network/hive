#pragma once

#include <hive/plugins/sql_serializer/data_dumper.h>

#include <hive/plugins/sql_serializer/table_data_writer.h>
#include <hive/plugins/sql_serializer/tables_descriptions.h>

#include <hive/plugins/sql_serializer/end_massive_sync_processor.hpp>
#include <hive/plugins/sql_serializer/cached_data.h>

#include <memory>
#include <string>

namespace hive::plugins::sql_serializer {

  class reindex_data_dumper: public data_dumper {
  public:
    reindex_data_dumper( const std::string& db_url );

    ~reindex_data_dumper() { join(); }
    reindex_data_dumper(reindex_data_dumper&) = delete;
    reindex_data_dumper(reindex_data_dumper&&) = delete;
    reindex_data_dumper& operator=(reindex_data_dumper&&) = delete;
    reindex_data_dumper& operator=(reindex_data_dumper&) = delete;

    void trigger_data_flush( cached_data_t& cached_data, int last_block_num ) override;
    void join() override;
    void wait_for_data_processing_finish() override;
    uint32_t blocks_per_flush() const override { return 1000; }
  private:
    using block_data_container_t_writer = table_data_writer<hive_blocks>;
    using transaction_data_container_t_writer = table_data_writer<hive_transactions>;
    using transaction_multisig_data_container_t_writer = table_data_writer<hive_transactions_multisig>;
    using operation_data_container_t_writer = table_data_writer<hive_operations>;

    std::unique_ptr< block_data_container_t_writer > _block_writer;
    std::unique_ptr< transaction_data_container_t_writer > _transaction_writer;
    std::unique_ptr< transaction_multisig_data_container_t_writer > _transaction_multisig_writer;
    std::unique_ptr< operation_data_container_t_writer > _operation_writer;
    std::unique_ptr<end_massive_sync_processor> _end_massive_sync_processor;
  };

} // namespace hive::plugins::sql_serializer

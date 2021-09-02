#pragma once

#include <hive/plugins/sql_serializer/data_dumper.h>

#include <hive/plugins/sql_serializer/table_data_writer.h>
#include <hive/plugins/sql_serializer/tables_descriptions.h>
#include <hive/plugins/sql_serializer/string_data_processor.h>

#include <hive/plugins/sql_serializer/cached_data.h>

#include <boost/signals2.hpp>

#include <memory>
#include <string>

namespace appbase { class abstract_plugin; }

namespace hive::chain {
  class database;
} // namespace hive::chain

namespace hive::plugins::sql_serializer {
  class transaction_controller;

  class livesync_data_dumper : public data_dumper {
  public:
    livesync_data_dumper( std::string db_url, const appbase::abstract_plugin& plugin, hive::chain::database& chain_db );

    ~livesync_data_dumper();
    livesync_data_dumper(livesync_data_dumper&) = delete;
    livesync_data_dumper(livesync_data_dumper&&) = delete;
    livesync_data_dumper& operator=(livesync_data_dumper&&) = delete;
    livesync_data_dumper& operator=(livesync_data_dumper&) = delete;

    void trigger_data_flush( cached_data_t& cached_data, int last_block_num ) override;
    void join() override;
    void wait_for_data_processing_finish() override;
    uint32_t blocks_per_flush() const override { return 1; }

  private:
    void on_irreversible_block( uint32_t block_num );
    void on_switch_fork( uint32_t block_num );

  private:
    using block_data_container_t_writer = table_data_writer<hive_blocks, string_data_processor>;
    using transaction_data_container_t_writer = table_data_writer<hive_transactions, string_data_processor>;
    using transaction_multisig_data_container_t_writer = table_data_writer<hive_transactions_multisig, string_data_processor>;
    using operation_data_container_t_writer = table_data_writer<hive_operations, string_data_processor>;

    std::unique_ptr< block_data_container_t_writer > _block_writer;
    std::unique_ptr< transaction_data_container_t_writer > _transaction_writer;
    std::unique_ptr< transaction_multisig_data_container_t_writer > _transaction_multisig_writer;
    std::unique_ptr< operation_data_container_t_writer > _operation_writer;

    std::string _block;
    std::string _transactions;
    std::string _transactions_multisig;
    std::string _operations;

    boost::signals2::connection _on_irreversible_block_conn;
    boost::signals2::connection _on_switch_fork_conn;
    std::shared_ptr< transaction_controller > transactions_controller;
  };

} // namespace hive::plugins::sql_serializer

#include <hive/plugins/sql_serializer/reindex_data_dumper.h>

namespace hive::plugins::sql_serializer {
  reindex_data_dumper::reindex_data_dumper( const std::string& db_url ) {
    _end_massive_sync_processor = std::make_unique< end_massive_sync_processor >( db_url );
    constexpr auto NUMBER_OF_PROCESSORS_THREADS = 4;
    auto execute_end_massive_sync_callback = [this](trigger_synchronous_masive_sync_call::BLOCK_NUM _block_num ){
      _end_massive_sync_processor->trigger_block_number( _block_num );
    };
    auto api_trigger = std::make_shared< trigger_synchronous_masive_sync_call >( NUMBER_OF_PROCESSORS_THREADS, execute_end_massive_sync_callback );

    _block_writer = std::make_unique<block_data_container_t_writer>(db_url, "Block data writer", api_trigger);
    _transaction_writer = std::make_unique<transaction_data_container_t_writer>(db_url, "Transaction data writer", api_trigger);
    _transaction_multisig_writer = std::make_unique<transaction_multisig_data_container_t_writer>(db_url, "Transaction multisig data writer", api_trigger);
    _operation_writer = std::make_unique<operation_data_container_t_writer>(db_url, "Operation data writer", api_trigger);
  }

  void reindex_data_dumper::trigger_data_flush( cached_data_t& cached_data, int last_block_num ) {
    _block_writer->trigger( std::move( cached_data.blocks ), false, last_block_num );
    _operation_writer->trigger( std::move( cached_data.operations ), false, last_block_num );
    _transaction_writer->trigger( std::move( cached_data.transactions ), false, last_block_num);
    _transaction_multisig_writer->trigger( std::move( cached_data.transactions_multisig ), false, last_block_num );
  }

  void reindex_data_dumper::join() {
    _block_writer->join();
    _transaction_writer->join();
    _transaction_multisig_writer->join();
    _operation_writer->join();
    _end_massive_sync_processor->join();
  }

  void reindex_data_dumper::wait_for_data_processing_finish()
  {
    _block_writer->complete_data_processing();
    _transaction_writer->complete_data_processing();
    _transaction_multisig_writer->complete_data_processing();
    _operation_writer->complete_data_processing();
    _end_massive_sync_processor->complete_data_processing();
  }
} // namespace hive::plugins::sql_serializer



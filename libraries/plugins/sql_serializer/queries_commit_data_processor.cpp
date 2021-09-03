#include <hive/plugins/sql_serializer/queries_commit_data_processor.h>

namespace hive{ namespace plugins{ namespace sql_serializer {
queries_commit_data_processor::queries_commit_data_processor(const std::string& psqlUrl, std::string description, const data_processing_fn& dataProcessor, std::shared_ptr< block_num_rendezvous_trigger > api_trigger ) {
  auto tx_controller = build_own_transaction_controller( psqlUrl );
  auto fn_wrapped_with_transaction = [ tx_controller, dataProcessor ]( const data_chunk_ptr& dataPtr ){
    transaction_ptr tx( tx_controller->openTx() );
    auto result = dataProcessor( dataPtr, *tx );
    tx->commit();

    return result;
  };

  m_wrapped_processor = std::make_unique< data_processor >( description, fn_wrapped_with_transaction, api_trigger );
}

queries_commit_data_processor::~queries_commit_data_processor() {
}

void
queries_commit_data_processor::trigger(data_chunk_ptr dataPtr, uint32_t last_blocknum) {
  m_wrapped_processor->trigger( std::move(dataPtr), last_blocknum );
}

void
queries_commit_data_processor::complete_data_processing() {
  m_wrapped_processor->complete_data_processing();
}

void
queries_commit_data_processor::cancel() {
  m_wrapped_processor->cancel();
}
void
queries_commit_data_processor::join() {
  m_wrapped_processor->join();
}

void
queries_commit_data_processor::only_report_batch_finished(uint32_t _block_num ) {
  m_wrapped_processor->only_report_batch_finished( _block_num );
}
}}} //namespace hive::plugins::sql_serializer
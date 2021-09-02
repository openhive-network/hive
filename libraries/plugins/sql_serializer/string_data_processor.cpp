#include <hive/plugins/sql_serializer/string_data_processor.h>

namespace hive::plugins::sql_serializer {

  string_data_processor::string_data_processor(
      callback string_callback
    , std::string description
    , data_processing_fn dataProcessor
    , std::shared_ptr< block_num_rendezvous_trigger > api_trigger
    )
    {
    auto fn_wrapped_with_transaction = [ string_callback, dataProcessor ]( const data_chunk_ptr& dataPtr ){
      auto result = dataProcessor( dataPtr, string_callback );
      return result;
    };

    m_wrapped_processor = std::make_unique< data_processor >( description, fn_wrapped_with_transaction, api_trigger );
    }

    string_data_processor::~string_data_processor() {
  }

  void
  string_data_processor::trigger(data_chunk_ptr dataPtr, uint32_t last_blocknum) {
    m_wrapped_processor->trigger( std::move(dataPtr), last_blocknum );
  }

  void
  string_data_processor::complete_data_processing() {
    m_wrapped_processor->complete_data_processing();
  }

  void
  string_data_processor::cancel() {
    m_wrapped_processor->cancel();
  }
  void
  string_data_processor::join() {
    m_wrapped_processor->join();
  }

  void
  string_data_processor::only_report_batch_finished(uint32_t _block_num ) {
    m_wrapped_processor->only_report_batch_finished( _block_num );
  }

} // namespace hive::plugins::sql_serializer


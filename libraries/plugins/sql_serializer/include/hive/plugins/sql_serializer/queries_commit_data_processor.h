#pragma once

#include <hive/plugins/sql_serializer/data_processor.hpp>

namespace hive::plugins::sql_serializer {
  class queries_commit_data_processor
    {
    public:
      using transaction_ptr = transaction_controller::transaction_ptr;
      using data_processing_fn = std::function<data_processor::data_processing_status(const data_processor::data_chunk_ptr& dataPtr, transaction& tx)>;
      using data_chunk_ptr = std::unique_ptr<data_processor::data_chunk>;

      /// pairs number of produced chunks and write status
      typedef std::pair<size_t, bool> data_processing_status;

      queries_commit_data_processor( const std::string& psqlUrl, std::string description, const data_processing_fn& dataProcessor, std::shared_ptr< block_num_rendezvous_trigger > api_trigger );
      ~queries_commit_data_processor();

      queries_commit_data_processor(data_processor&&) = delete;
      queries_commit_data_processor& operator=(data_processor&&) = delete;
      queries_commit_data_processor(const data_processor&) = delete;
      queries_commit_data_processor& operator=(const data_processor&) = delete;


      void trigger(data_chunk_ptr dataPtr, uint32_t last_blocknum);
      /// Allows to hold execution of calling thread, until data processing thread will consume data and starts awaiting for another trigger call.
      void complete_data_processing();
      void cancel();
      void join();
      void only_report_batch_finished( uint32_t _block_num );
    private:
      transaction_controller_ptr _txController;
      std::unique_ptr< data_processor > m_wrapped_processor;
    };
} // namespace hive::plugins::sql_serializer

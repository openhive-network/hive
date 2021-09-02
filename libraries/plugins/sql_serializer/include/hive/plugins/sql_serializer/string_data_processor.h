#pragma once

#include <functional>

#include <hive/plugins/sql_serializer/data_processor.hpp>

namespace hive::plugins::sql_serializer {

  class string_data_processor
    {
    public:
      using callback = std::function<void(std::string&&)>;
      using data_processing_fn = std::function<data_processor::data_processing_status(
        const data_processor::data_chunk_ptr&
        , callback
        )
          >;
      using data_chunk_ptr = std::unique_ptr<data_processor::data_chunk>;

      /// pairs number of produced chunks and write status
      typedef std::pair<size_t, bool> data_processing_status;

      string_data_processor(
        callback string_callback
        , std::string description
        , data_processing_fn dataProcessor
        , std::shared_ptr< block_num_rendezvous_trigger > api_trigger
        );
      ~string_data_processor();

      string_data_processor(data_processor&&) = delete;
      string_data_processor& operator=(data_processor&&) = delete;
      string_data_processor(const data_processor&) = delete;
      string_data_processor& operator=(const data_processor&) = delete;


      void trigger(data_chunk_ptr dataPtr, uint32_t last_blocknum);
      /// Allows to hold execution of calling thread, until data processing thread will consume data and starts awaiting for another trigger call.
      void complete_data_processing();
      void cancel();
      void join();
      void only_report_batch_finished( uint32_t _block_num );
    private:
      std::unique_ptr< data_processor > m_wrapped_processor;
    };

} // namespace hive::plugins::sql_serializer


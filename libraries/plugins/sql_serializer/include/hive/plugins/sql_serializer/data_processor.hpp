#pragma once

#include <hive/plugins/sql_serializer/transaction_controllers.hpp>
#include <hive/plugins/sql_serializer/block_num_rendezvous_trigger.hpp>

#include <fc/optional.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>


namespace hive { namespace plugins { namespace sql_serializer {

class data_processor 
{
public:
  class data_chunk
  {
  public:
    virtual ~data_chunk() = default;
  };

  using transaction_ptr = transaction_controller::transaction_ptr;

  typedef std::unique_ptr<data_chunk> data_chunk_ptr;

  /// pairs number of produced chunks and write status
  typedef std::pair<size_t, bool> data_processing_status;
  typedef std::function<data_processing_status(const data_chunk_ptr& dataPtr)> data_processing_fn;

  data_processor( std::string description, data_processing_fn dataProcessor, std::shared_ptr< block_num_rendezvous_trigger > api_trigger );
  ~data_processor();

  data_processor(data_processor&&) = delete;
  data_processor& operator=(data_processor&&) = delete;
  data_processor(const data_processor&) = delete;
  data_processor& operator=(const data_processor&) = delete;

  void trigger(data_chunk_ptr dataPtr, uint32_t last_blocknum);
  //where there the chunk is empty, only confirms that the bunch of blocks was processed by the processor
  void only_report_batch_finished( uint32_t _block_num );
  /// Allows to hold execution of calling thread, until data processing thread will consume data and starts awaiting for another trigger call.
  void complete_data_processing();
  void cancel();
  void join();

  private:
    std::string _description;
    std::atomic_bool _cancel;
    std::atomic_bool _continue;
    std::atomic_bool _initialized;
    std::atomic_bool _finished;
    std::atomic_bool _is_processing_data;

    std::thread _worker;
    std::mutex _mtx;
    std::mutex _data_processing_mtx;
    std::condition_variable _cv;
    std::condition_variable _data_processing_finished_cv;
    fc::optional<data_chunk_ptr> _dataPtr;
    uint32_t _last_block_num = 0;

    size_t _total_processed_records;

    std::shared_ptr< block_num_rendezvous_trigger > _api_trigger;
};


class queries_commit_data_processor
{
  public:
    using transaction_ptr = transaction_controller::transaction_ptr;
    using data_processing_fn = std::function<data_processor::data_processing_status(const data_processor::data_chunk_ptr& dataPtr, transaction& tx)>;
    using data_chunk_ptr = std::unique_ptr<data_processor::data_chunk>;

    /// pairs number of produced chunks and write status
    typedef std::pair<size_t, bool> data_processing_status;

    queries_commit_data_processor(std::string psqlUrl, std::string description, data_processing_fn dataProcessor, std::shared_ptr< block_num_rendezvous_trigger > api_trigger );
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

}}} /// hive::plugins::sql_serializer


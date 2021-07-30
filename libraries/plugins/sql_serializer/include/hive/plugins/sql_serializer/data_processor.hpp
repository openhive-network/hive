#pragma once

#include <hive/plugins/sql_serializer/transaction_controllers.hpp>

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
    virtual std::string generate_code(size_t* processedItem) const = 0;
    
    virtual ~data_chunk() {}
  };

  using transaction_ptr = transaction_controller::transaction_ptr;

  typedef std::unique_ptr<data_chunk> data_chunk_ptr;

  /// pairs number of produced chunks and write status
  typedef std::pair<size_t, bool> data_processing_status;
  typedef std::function<data_processing_status(const data_chunk_ptr& dataPtr, transaction& openedTx)> data_processing_fn;

  data_processor(std::string psqlUrl, std::string description, data_processing_fn dataProcessor);
  ~data_processor();

  data_processor(data_processor&&) = delete;
  data_processor& operator=(data_processor&&) = delete;

  data_processor(const data_processor&) = delete;
  data_processor& operator=(const data_processor&) = delete;

  transaction_controller_ptr register_transaction_controler(transaction_controller_ptr controller);

  void trigger(data_chunk_ptr dataPtr);
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
    transaction_controller_ptr _txController;

    size_t _total_processed_records;
};

}}} /// hive::plugins::sql_serializer


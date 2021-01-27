#pragma once

// C++ connector library for PostgreSQL (http://pqxx.org/development/libpqxx/)
#include <pqxx/pqxx>

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

  typedef std::unique_ptr<data_chunk> data_chunk_ptr;

  /// pairs number of produced chunks and write status
  typedef std::pair<size_t, bool> data_processing_status;
  typedef std::function<data_processing_status(const data_chunk_ptr& dataPtr, pqxx::work& openedTx)> data_processing_fn;

  data_processor(std::string psqlUrl, std::string description, data_processing_fn dataProcessor);
  ~data_processor();

  void trigger(data_chunk_ptr dataPtr);
  void cancel();
  void join();

  private:    
    std::string _description;
    std::atomic_bool _cancel;
    std::atomic_bool _continue;

    std::thread _worker;
    std::mutex _mtx;
    std::condition_variable _cv;
    fc::optional<data_chunk_ptr> _dataPtr;

    size_t _total_processed_records;
};

}}} /// hive::plugins::sql_serializer


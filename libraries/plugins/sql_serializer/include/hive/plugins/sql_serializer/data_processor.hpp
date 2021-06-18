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

  /**
   * @brief Represents opened internal transaction.
     Can be explicitly commited or rollbacked. If not explicitly committed, implicit rollback is performed at destruction time.
  */
  class transaction
  {
  public:
    virtual void commit() = 0;
    virtual void exec() = 0;
    virtual void rollback() = 0;

    virtual ~transaction() = 0;
  };

  class transaction_controller
  {
  public:
    typedef std::unique_ptr<transaction> transaction_ptr;
    /// Opens internal transaction. \see transaction class for further description.
    virtual transaction_ptr openTx() = 0;
  };
  typedef std::unique_ptr<transaction_controller> transaction_controller_ptr;
  typedef std::unique_ptr<data_chunk> data_chunk_ptr;

  /// pairs number of produced chunks and write status
  typedef std::pair<size_t, bool> data_processing_status;
  typedef std::function<data_processing_status(const data_chunk_ptr& dataPtr, transaction& openedTx)> data_processing_fn;

  data_processor(std::string psqlUrl, std::string description, data_processing_fn dataProcessor);
  ~data_processor();

  transaction_controller_ptr register_transaction_controler(transaction_controller_ptr controller);

  void trigger(data_chunk_ptr dataPtr);
  void cancel();
  void join();

  private:
    std::string _description;
    std::atomic_bool _cancel;
    std::atomic_bool _continue;
    std::atomic_bool _finished;

    std::thread _worker;
    std::mutex _mtx;
    std::condition_variable _cv;
    fc::optional<data_chunk_ptr> _dataPtr;
    transaction_controller_ptr _txController;

    size_t _total_processed_records;
};

}}} /// hive::plugins::sql_serializer


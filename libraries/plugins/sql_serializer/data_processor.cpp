#include <hive/plugins/sql_serializer/data_processor.hpp>

#include <fc/log/logger.hpp>

#include <exception>

namespace hive { namespace plugins { namespace sql_serializer {

data_processor::data_processor(std::string psqlUrl, std::string description, data_processing_fn dataProcessor) :
  _description(std::move(description)),
  _cancel(false),
  _continue(true),
  _total_processed_records(0)
{
  auto body = [this, psqlUrl, dataProcessor]() -> void
  {
    ilog("Entering data processor thread: ${d}", ("d", _description));

    try
    {
      ilog("${d} data processor is connecting to specified db url: `${url}' ...", ("url", psqlUrl)("d", _description));
      pqxx::connection dbConnection(psqlUrl);
    
      ilog("${d} data processor connecting successfully ...", ("d", _description));

      while(_continue.load())
      {
        dlog("${d} data processor is waiting for DATA-READY signal...", ("d", _description));
        std::unique_lock<std::mutex> lk(_mtx);
        _cv.wait(lk, [this] {return _dataPtr.valid() || _continue.load() == false; });

        dlog("${d} data processor resumed by DATA-READY signal...", ("d", _description));

        if(_continue.load() == false)
          break;

        fc::optional<data_chunk_ptr> dataPtr(std::move(_dataPtr));

        lk.unlock();
        dlog("${d} data processor consumed data - notifying trigger process...", ("d", _description));
        _cv.notify_one();

        if(_cancel.load())
          break;

        dlog("${d} data processor starts a data processing...", ("d", _description));

        {
          pqxx::work tx(dbConnection);

          dataProcessor(*dataPtr, tx);

          tx.commit();
        }

        dlog("${d} data processor finished processing a data chunk...", ("d", _description));
      }

      dbConnection.disconnect();
    }
    catch(const pqxx::pqxx_exception& ex)
    {
      elog("Data processor ${d} detected SQL execution failure: ${e}", ("d", _description)("e", ex.base().what()));
    }
    catch(const std::exception& ex)
    {
      elog("Data processor ${d} execution failed: ${e}", ("d", _description)("e", ex.what()));
    }

    ilog("Leaving data processor thread: ${d}", ("d", _description));
  };

  _worker = std::thread(body);
}

data_processor::~data_processor()
{

}


void data_processor::trigger(data_chunk_ptr dataPtr)
{
  {
  dlog("Trying to trigger data processor: ${d}...", ("d", _description));
  std::lock_guard<std::mutex> lk(_mtx);
  _dataPtr = std::move(dataPtr);
  dlog("Data processor: ${d} triggerred...", ("d", _description));
  }
  _cv.notify_one();

  /// wait for the worker
  {
    dlog("Waiting until data_processor ${d} will consume a data...", ("d", _description));
    std::unique_lock<std::mutex> lk(_mtx);
    _cv.wait(lk, [this] {return _dataPtr.valid() == false; });
  }


  dlog("Leaving trigger of data data processor: ${d}...", ("d", _description));
}

void data_processor::cancel()
{
  ilog("Attempting to cancel execution of data processor: ${d}...", ("d", _description));

  _cancel.store(true);
  join();
}

void data_processor::join()
{
  _continue.store(false);

  {
    ilog("Trying to resume data processor: ${d}...", ("d", _description));
    std::lock_guard<std::mutex> lk(_mtx);
    ilog("Data processor: ${d} triggerred...", ("d", _description));
  }
  _cv.notify_one();

  ilog("Waiting for data processor: ${d} worker thread finish...", ("d", _description));

  if(_worker.joinable())
    _worker.join();

  ilog("Data processor: ${d} finished execution...", ("d", _description));
}


}}} /// hive::plugins::sql_serializer

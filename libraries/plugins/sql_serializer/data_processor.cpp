#include <hive/plugins/sql_serializer/data_processor.hpp>

#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <exception>

namespace hive { namespace plugins { namespace sql_serializer {

class data_processing_status_notifier
{
public:
  data_processing_status_notifier(std::atomic_bool* is_processing_flag, std::mutex* mtx, std::condition_variable* notifier) :
  _lk(*mtx),
  _is_processing_flag(is_processing_flag),
  _notifier(notifier)
  {
    _is_processing_flag->store(true);
  }

  ~data_processing_status_notifier()
  {
    _is_processing_flag->store(false);
    _notifier->notify_one();
  }

private:
  std::unique_lock<std::mutex> _lk;
  std::atomic_bool* _is_processing_flag;
  std::condition_variable* _notifier;
};

data_processor::data_processor( std::string description, const data_processing_fn& dataProcessor, std::shared_ptr< block_num_rendezvous_trigger > api_trigger) :
  _description(std::move(description)),
  _cancel(false),
  _continue(true),
  _initialized(false),
  _finished(false),
  _is_processing_data( false ),
  _total_processed_records(0),
  _randezvous_trigger( std::move( api_trigger ) )
{
  auto body = [this, dataProcessor]() -> void
  {
    ilog("Entering data processor thread: ${d}", ("d", _description));

    try
    {
      {
        ilog("${d} data processor is connecting ...",("d", _description));
        std::unique_lock<std::mutex> lk(_mtx);
        _initialized = true;
        ilog("${d} data processor connected successfully ...", ("d", _description));
        _cv.notify_one();
      }

      while(_continue.load())
      {
        dlog("${d} data processor is waiting for DATA-READY signal...", ("d", _description));
        std::unique_lock<std::mutex> lk(_mtx);
        _cv.wait(lk, [this] {return _dataPtr.valid() || _continue.load() == false; });

        dlog("${d} data processor resumed by DATA-READY signal...", ("d", _description));

        if(_continue.load() == false) {
          dlog("${d} data processor _continue.load() == false", ("d", _description));
          break;
        }

        fc::optional<data_chunk_ptr> dataPtr(std::move(_dataPtr));
        uint32_t last_block_num_in_stage = _last_block_num;

        lk.unlock();
        dlog("${d} data processor consumed data - notifying trigger process...", ("d", _description));
        _cv.notify_one();

        if(_cancel.load())
          break;

        dlog("${d} data processor starts a data processing...", ("d", _description));

        {
          data_processing_status_notifier notifier(&_is_processing_data, &_data_processing_mtx, &_data_processing_finished_cv);

          dataProcessor(*dataPtr);

          if ( _randezvous_trigger && last_block_num_in_stage )
            _randezvous_trigger->report_complete_thread_stage( last_block_num_in_stage );
        }

        dlog("${d} data processor finished processing a data chunk...", ("d", _description));
      }
    }
    catch(const pqxx::sql_error& ex)
    {
      //elog("Data processor ${d} detected SQL statement execution failure. Failing statement: `${q}', SQLState: ${s}.", ("d", _description)("q", ex.query())("s", ex.sqlstate()));
      elog("Data processor ${d} detected SQL statement execution failure. Failing statement: `${q}'.", ("d", _description)("q", ex.query()));
    }
    catch(const pqxx::pqxx_exception& ex)
    {
      elog("Data processor ${d} detected SQL execution failure: ${e}", ("d", _description)("e", ex.base().what()));
    }
    catch(const fc::exception& ex)
    {
      elog("Data processor ${d} execution failed: ${e}", ("d", _description)("e", ex.what()));
    }
    catch(const std::exception& ex)
    {
      elog("Data processor ${d} execution failed: ${e}", ("d", _description)("e", ex.what()));
    }

    ilog("Leaving data processor thread: ${d}", ("d", _description));
    _finished = true;
  };

  _future = std::async(std::launch::async, body);
}

data_processor::~data_processor()
{
}

void data_processor::trigger(data_chunk_ptr dataPtr, uint32_t last_blocknum)
{
  if(_finished)
    return;

  /// Set immediately data processing flag
  _is_processing_data = true;

  {
  dlog("Trying to trigger data processor: ${d}...", ("d", _description));
  std::lock_guard<std::mutex> lk(_mtx);
  _dataPtr = std::move(dataPtr);
  _last_block_num = last_blocknum;
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

void
data_processor::only_report_batch_finished( uint32_t block_num ) {
  if ( _randezvous_trigger ) {
    ilog( "${i} commited by ${d}",("i", block_num )("d", _description) );
    _randezvous_trigger->report_complete_thread_stage( block_num );
  }
}

void data_processor::complete_data_processing()
{
  if(_is_processing_data == false)
    return;

  dlog("Awaiting for data processing finish in the  data processor: ${d}...", ("d", _description));
  std::unique_lock<std::mutex> lk(_data_processing_mtx);
  _data_processing_finished_cv.wait(lk, [this] { return _is_processing_data == false; });
  dlog("Data processor: ${d} finished processing data...", ("d", _description));
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

  try {
    if( _future.valid() )
      _future.get();
  } catch (...) {
    elog( "Caught unhandled exception ${diagnostic}", ("diagnostic", boost::current_exception_diagnostic_information()) );
  }

  ilog("Data processor: ${d} finished execution...", ("d", _description));
}

}}} /// hive::plugins::sql_serializer

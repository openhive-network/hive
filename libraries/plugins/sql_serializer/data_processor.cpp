#include <hive/plugins/sql_serializer/data_processor.hpp>

#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

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

data_processor::data_processor(std::string psqlUrl, std::string description, data_processing_fn dataProcessor, std::shared_ptr< block_num_rendezvous_trigger > api_trigger) :
  _description(std::move(description)),
  _cancel(false),
  _continue(true),
  _initialized(false),
  _finished(false),
  _is_processing_data( false ),
  _total_processed_records(0),
  _api_trigger( api_trigger )
{
  auto body = [this, psqlUrl, dataProcessor]() -> void
  {
    ilog("Entering data processor thread: ${d}", ("d", _description));

    try
    {
      {
        ilog("${d} data processor is connecting to specified db url: `${url}' ...", ("url", psqlUrl)("d", _description));
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

          if ( _api_trigger && last_block_num_in_stage )
            _api_trigger->report_complete_thread_stage( last_block_num_in_stage );
        }

        dlog("${d} data processor finished processing a data chunk...", ("d", _description));
      }
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

  _worker = std::thread(body);
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
  if ( _api_trigger ) {
    ilog( "${i} commited by ${d}",("i", block_num )("d", _description) );
    _api_trigger->report_complete_thread_stage( block_num );
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

  if(_finished == false && _worker.joinable())
    _worker.join();

  ilog("Data processor: ${d} finished execution...", ("d", _description));
}

queries_commit_data_processor::queries_commit_data_processor(std::string psqlUrl, std::string description, data_processing_fn dataProcessor, std::shared_ptr< block_num_rendezvous_trigger > api_trigger ) {
  auto tx_controller = build_own_transaction_controller( psqlUrl );
  auto fn_wrapped_with_transaction = [ tx_controller, dataProcessor ]( const data_chunk_ptr& dataPtr ){
    transaction_ptr tx( tx_controller->openTx() );
    auto result = dataProcessor( dataPtr, *tx );
    tx->commit();

    return result;
  };

  m_wrapped_processor = std::make_unique< data_processor >( psqlUrl, description, fn_wrapped_with_transaction, api_trigger );
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

}}} /// hive::plugins::sql_serializer

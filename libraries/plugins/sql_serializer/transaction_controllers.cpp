#include <hive/plugins/sql_serializer/transaction_controllers.hpp>

#include <fc/exception/exception.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

namespace hive { namespace plugins { namespace sql_serializer {

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////
///                 Own transaction controler (thread/connection specific)                       ///
////////////////////////////////////////////////////////////////////////////////////////////////////

class own_tx_controller final : public transaction_controller
{
public:
  own_tx_controller(const std::string& dbUrl, const std::string& description) : _dbUrl(dbUrl), _description(description) {}

/// transaction_controller:
  virtual transaction_ptr openTx() override;
  virtual void disconnect() override;

private:
  class own_transaction final : public transaction
  {
  public:
    own_transaction(own_tx_controller* owner) : _owner(owner) 
    {
      FC_ASSERT(_owner->_opened_tx == nullptr);
      _owner->_opened_tx = this;

      with_retry([]() -> void
        {
        /// Nothing to do here - transaction will opened inside reconnect
        }
      );
    }

    virtual ~own_transaction()
    {
      do_rollback();
    }

    virtual void commit() override;
    virtual pqxx::result exec(const std::string& query) override;
    virtual void rollback() override;

  private:
    template <typename Executor>
    auto with_retry(Executor ex) -> decltype(ex())
    {
      unsigned int retry = 0;
      const unsigned int MAX_RETRY_COUNT = 100;

      pqxx::broken_connection copy;

      do
      {
        try
        {
          if(_opened_tx == nullptr)
            do_reconnect();

          return ex();
        }
        catch(const pqxx::broken_connection& ex)
        {
          _opened_tx.reset();

          wlog("Transaction controller: `${d}' lost connection to database: `${url}'. Retrying # ${r}...", ("d", _owner->_description)("url", _owner->_dbUrl)("r", retry));
          ++retry;

          copy = ex;

          /// Give a chance to restart server or somehow "repair" it
          using namespace std::chrono_literals;
          std::this_thread::sleep_for(500ms);
        }
      } while(retry < MAX_RETRY_COUNT)
      ;

      elog("Transaction controller: `${d}' permanently lost connection to database: `${url}'. Exiting.", ("d", _owner->_description)("url", _owner->_dbUrl));

      throw copy;
    }


    void do_reconnect()
    {
      try
      {
        _opened_tx.reset();
        if(_owner->_opened_connection != nullptr)
        {
          _owner->_opened_connection->disconnect();
          _owner->_opened_connection.release();
        }
      }
      catch(const pqxx::pqxx_exception& ex)
      {
        ilog("Ignoring a pqxx exception during an implicit disconnect forced by reconnect request: ${e}", ("e", ex.base().what()));
      }

      dlog("Trying to connect to database: `${url}'...", ("url", _owner->_dbUrl));
      _owner->_opened_connection = std::make_unique<pqxx::connection>(_owner->_dbUrl);
      _opened_tx = std::make_unique<pqxx::work>(*_owner->_opened_connection.get());
      dlog("Connected to database: `${url}'.", ("url", _owner->_dbUrl));

    }

    void finalize_transaction()
    {
      if(_owner)
      {
        FC_ASSERT(_owner->_opened_tx == this);
        _owner->_opened_tx = nullptr;
        _owner = nullptr;
      }
    }

    void do_rollback()
    {
      if(_opened_tx)
      {
        _opened_tx->abort();
        _opened_tx.reset();
      }
      
      finalize_transaction();
    }

  private:
    own_tx_controller*          _owner;
    std::unique_ptr<pqxx::work> _opened_tx;
  };

private:
  const std::string _dbUrl;
  const std::string _description;

  std::unique_ptr<pqxx::connection> _opened_connection;
  own_transaction* _opened_tx = nullptr;
};


void own_tx_controller::own_transaction::commit()
{
  if(_opened_tx)
  {
    _opened_tx->commit();
    _opened_tx.reset();
  }

  finalize_transaction();
}

pqxx::result own_tx_controller::own_transaction::exec(const std::string& query)
{
  return with_retry([this, &query]() -> pqxx::result
    {
      FC_ASSERT(_opened_tx, "No transaction opened");

      return _opened_tx->exec(query);
    }
  );
}

void own_tx_controller::own_transaction::rollback()
{
  do_rollback();
}

transaction_controller::transaction_ptr own_tx_controller::openTx()
{
  return std::make_unique<own_transaction>(this);
}

void own_tx_controller::disconnect()
{
  if(_opened_connection)
  {
    if(_opened_tx)
      _opened_tx->rollback();

    _opened_connection->disconnect();
    _opened_connection.release();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///  Single transaction controler (sharing connection and executing SQL commands sequentially)   ///
////////////////////////////////////////////////////////////////////////////////////////////////////

class single_transaction_controller : public transaction_controller
{
public:
  explicit single_transaction_controller(const std::string& dbUrl) : _clientCount(0)
  {
    _own_contoller = build_own_transaction_controller(dbUrl, "single_transaction_controller");
  }

/// transaction_controller:
  virtual transaction_ptr openTx() override;
  virtual void disconnect() override;

  void initialize_tx()
  {
    ++_clientCount;
    if(_clientCount == 1)
    {
      FC_ASSERT(_own_tx == nullptr, "Only single transaction allowed");
      _own_tx = _own_contoller->openTx();
    }
  }

  unsigned int finalize_tx()
  {
    --_clientCount;
    if(_clientCount == 0 && _own_tx)
      _own_tx.reset();

    return _clientCount;
  }

  pqxx::result do_query(const std::string& query)
  {
    if(_own_tx)
      return _own_tx->exec(query);

  return pqxx::result();
  }

  void do_commit()
  {
    if(_clientCount == 1 && _own_tx)
    {
      _own_tx->commit();
      _own_tx.reset();
    }
  }

  void do_rollback()
  {
    if(_own_tx)
    {
      _own_tx->rollback();
      _own_tx.reset();
    }
  }

private:
  /// Represents a fake transaction object which always delegates calls to the actual one.
  /// Also, by using dedicated mutex prevents on multiple calls made to exec between openTx/disconnect calls pair.
  class transaction_wrapper final : public transaction
  {
  public:
    transaction_wrapper(single_transaction_controller& owner, std::mutex& mtx) :
      _owner(owner), _locked_transaction(mtx), _do_implicit_rollback(true)
      {
        _owner.initialize_tx();
      }

    virtual ~transaction_wrapper();

    virtual void commit() override;
    virtual pqxx::result exec(const std::string& query) override;
    virtual void rollback() override;

  private:
    single_transaction_controller& _owner;
    std::unique_lock<std::mutex> _locked_transaction;
    bool _do_implicit_rollback;
  };

private:
  transaction_controller_ptr _own_contoller;
  transaction_ptr _own_tx;
  std::atomic_uint _clientCount;
  std::mutex _lock;
};

void single_transaction_controller::transaction_wrapper::commit()
{
  _owner.do_commit();
  _do_implicit_rollback = false;
}

pqxx::result single_transaction_controller::transaction_wrapper::exec(const std::string& query)
{
  return _owner.do_query(query);
}

void single_transaction_controller::transaction_wrapper::rollback()
{
  _owner.do_rollback();
  _do_implicit_rollback = false;
}

single_transaction_controller::transaction_wrapper::~transaction_wrapper()
{
  if(_do_implicit_rollback)
    _owner.do_rollback();

  _owner.finalize_tx();
}


transaction_controller::transaction_ptr single_transaction_controller::openTx()
{
  return std::make_unique<transaction_wrapper>(*this, _lock);
}

void single_transaction_controller::disconnect()
{
  /// If multiple clients use this controller, let them finish their work...
  if(_clientCount == 0 && _own_contoller)
    _own_contoller->disconnect();
}


} /// anonymous

transaction_controller_ptr build_own_transaction_controller(const std::string& dbUrl, const std::string& description)
{
  return std::make_shared<own_tx_controller>(dbUrl, description);
}

transaction_controller_ptr build_single_transaction_controller(const std::string& dbUrl)
{
  return std::make_shared<single_transaction_controller>(dbUrl);
}

} } } /// hive::plugins::sql_serializer


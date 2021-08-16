#include <hive/plugins/sql_serializer/transaction_controllers.hpp>

#include <fc/exception/exception.hpp>

#include <atomic>
#include <mutex>
#include <string>

namespace hive { namespace plugins { namespace sql_serializer {

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////
///                 Own transaction controler (thread/connection specific)                       ///
////////////////////////////////////////////////////////////////////////////////////////////////////

class own_tx_controller final : public transaction_controller
{
public:
  explicit own_tx_controller(const std::string& dbUrl) : _dbUrl(dbUrl) {}

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

      _opened_tx = std::make_unique<pqxx::work>(*_owner->_opened_connection.get());
    }

    virtual ~own_transaction()
    {
      do_rollback();
    }

    virtual void commit() override;
    virtual pqxx::result exec(const std::string& query) override;
    virtual void rollback() override;

  private:
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
  FC_ASSERT(_opened_tx, "No transaction opened");

  return _opened_tx->exec(query); 
}

void own_tx_controller::own_transaction::rollback()
{
  do_rollback();
}

transaction_controller::transaction_ptr own_tx_controller::openTx()
{
  if(!_opened_connection)
    _opened_connection = std::make_unique<pqxx::connection>(_dbUrl);

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
    _own_contoller = build_own_transaction_controller(dbUrl);
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

transaction_controller_ptr build_own_transaction_controller(const std::string& dbUrl)
{
  return std::make_shared<own_tx_controller>(dbUrl);
}

transaction_controller_ptr build_single_transaction_controller(const std::string& dbUrl)
{
  return std::make_shared<single_transaction_controller>(dbUrl);
}

} } } /// hive::plugins::sql_serializer


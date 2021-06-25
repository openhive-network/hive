#include <hive/plugins/sql_serializer/transaction_controllers.hpp>

#include <fc/exception/exception.hpp>

#include <string>

namespace hive { namespace plugins { namespace sql_serializer {

namespace {

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
  if(_opened_connection)
    _opened_connection->activate();
  else
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

} /// anonymous

transaction_controller_ptr build_own_transaction_controller(const std::string& dbUrl)
{
  return std::make_shared<own_tx_controller>(dbUrl);
}

} } } /// hive::plugins::sql_serializer


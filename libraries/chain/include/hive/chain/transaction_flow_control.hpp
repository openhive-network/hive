#pragma once
#include <hive/chain/full_transaction.hpp>

#include <fc/time.hpp>
#include <fc/thread/future.hpp>

#include <boost/thread/future.hpp>

namespace appbase {
  class application;
}

namespace hive { namespace chain {

/**
 * Wrapper for full_transaction to control transaction processing (and possibly collect statistics).
 * Covers differences in handling when transaction is coming from p2p/API or from colony plugin.
 */
class transaction_flow_control
{
public:
  explicit transaction_flow_control( const std::shared_ptr<full_transaction_type>& _tx ) : tx( _tx ) {}
  virtual ~transaction_flow_control() {}

  typedef fc::static_variant<std::shared_ptr<boost::promise<void>>, fc::promise<void>::ptr> promise_ptr;
  void attach_promise( promise_ptr _p ) { prom_ptr = _p; }

  void on_failure( const fc::exception& ex ) { except = ex.dynamic_copy_exception(); }
  virtual void on_worker_done( appbase::application& app ) const;

  const std::shared_ptr<full_transaction_type>& get_full_transaction() const { return tx; }
  const fc::exception_ptr& get_exception() const { return except; }
  void rethrow_if_exception() const { if( except ) except->dynamic_rethrow_exception(); }

private:
  std::shared_ptr<full_transaction_type> tx;
  promise_ptr                            prom_ptr;
  fc::exception_ptr                      except;
};

} } // hive::chain

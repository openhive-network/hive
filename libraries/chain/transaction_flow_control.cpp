#include <hive/chain/transaction_flow_control.hpp>

namespace hive { namespace chain {

struct request_promise_visitor
{
  request_promise_visitor() {}

  typedef void result_type;

  // n.b. our visitor functions take a shared pointer to the promise by value
  // so the promise can't be garbage collected until the set_value() function
  // has finished exiting.  There's a race where the calling thread wakes up
  // during the set_value() call and can delete the promise out from under us
  // unless we hold a reference here.
  void operator()( fc::promise<void>::ptr t )
  {
    t->set_value();
  }

  void operator()( std::shared_ptr<boost::promise<void>> t )
  {
    t->set_value();
  }
};

void transaction_flow_control::on_worker_done( appbase::application& app ) const
{
  request_promise_visitor prom_visitor;
  prom_ptr.visit( prom_visitor );
}

} } // hive::chain

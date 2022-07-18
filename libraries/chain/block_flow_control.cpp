#include <hive/chain/block_flow_control.hpp>

#include <hive/utilities/notifications.hpp>

namespace hive { namespace chain {

void block_flow_control::on_failure( const fc::exception& e ) const
{
  except = e.dynamic_copy_exception();
}

void block_flow_control::on_worker_done() const
{
}

void new_block_flow_control::on_failure( const fc::exception& e ) const
{
  block_flow_control::on_failure( e );
  //trigger_promise();
}

void p2p_block_flow_control::on_failure( const fc::exception& e ) const
{
  block_flow_control::on_failure( e );
  //trigger_promise();
}

void existing_block_flow_control::on_failure( const fc::exception& e ) const
{
  block_flow_control::on_failure( e );
}

} } // hive::chain

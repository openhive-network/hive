#include <hive/chain/block_flow_control.hpp>

#include <hive/utilities/notifications.hpp>

namespace hive { namespace chain {

void block_flow_control::on_fork_db_insert() const
{
  current_phase = phase::FORK_DB;
}

void block_flow_control::on_end_of_apply_block() const
{
  current_phase = phase::APPLIED;
}

void block_flow_control::on_failure( const fc::exception& e ) const
{
  except = e.dynamic_copy_exception();
}

void block_flow_control::on_worker_done() const
{
  log_stats();
}

void block_flow_control::log_stats() const
{
  const char* type = "";
  if( !finished() )
    type = "broken";
  else if( forked() )
    type = "forked";
  else if( ignored() )
    type = "ignored";
  else
    type = buffer_type();

  const auto& block_header = full_block->get_block_header();
  auto block_ts = block_header.timestamp;
  ilog( "Block stats:"
    "{\"num\":${nr},\"type\":\"${tp}\",\"id\":\"${id}\",\"ts\":\"${ts}\",\"bp\":\"${wit}\",\"txs\":${tx},\"size\":${s},\"offset\":${o},"
    "\"before\":{\"inc\":${bi},\"ok\":${bo}},"
    "\"after\":{\"exp\":${ae},\"fail\":${af},\"appl\":${aa},\"post\":${ap}},"
    "\"exec\":{\"offset\":${ef},\"pre\":${er},\"work\":${ew},\"post\":${eo},\"all\":${ea}}}",
    ( "nr", full_block->get_block_num() )( "tp", type )( "id", full_block->get_block_id() )( "ts", block_ts )
    ( "wit", block_header.witness )( "tx", full_block->get_full_transactions().size() )( "s", full_block->get_uncompressed_block_size() )
    ( "o", stats.get_ready_ts() - block_ts )
    ( "bi", stats.get_txs_processed_before_block() )( "bo", stats.get_txs_accepted_before_block() )
    ( "ae", stats.get_txs_expired_after_block() )( "af", stats.get_txs_failed_after_block() )
    ( "aa", stats.get_txs_reapplied_after_block() )( "ap", stats.get_txs_postponed_after_block() )
    ( "ef", stats.get_creation_ts() - block_ts )
    ( "er", stats.get_wait_time() )( "ew", stats.get_work_time() )
    ( "eo", stats.get_cleanup_time() )( "ea", stats.get_total_time() )
  );
}

void block_flow_control::notify_stats() const
{
  //TODO: supplement
  //hive::notify( "Block stats",
  // "num", block->block_num(),
  // "type", ...
  //);
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

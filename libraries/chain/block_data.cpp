#include <hive/chain/block_data.hpp>

namespace hive { namespace chain {

void block_data::on_fork_db_insert()
{
  current_phase = phase::FORK_DB;
}

void block_data::on_end_of_apply_block()
{
  current_phase = phase::APPLIED;
}

void block_data::on_failure( const fc::exception& e )
{
  except = e;
}

void block_data::on_worker_done()
{
  log_stats();
}

void block_data::log_stats() const
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

  ilog( "Block stats:"
    "{\"num\":${nr},\"type\":\"${tp}\",\"id\":\"${id}\",\"ts\":\"${ts}\",\"bp\":\"${wit}\",\"txs\":${tx},\"size\":${s},\"offset\":${o},"
    "\"before\":{\"inc\":${bi},\"ok\":${bo}},"
    "\"after\":{\"exp\":${ae},\"fail\":${af},\"appl\":${aa},\"post\":${ap}},"
    "\"exec\":{\"offset\":${ef},\"pre\":${er},\"work\":${ew},\"post\":${eo},\"all\":${ea}}}",
    ( "nr", block->block_num() )( "tp", type )( "id", block->id() )( "ts", block->timestamp )
    ( "wit", block->witness )( "tx", block->transactions.size() )( "s", block_size )
    ( "o", stats.get_ready_ts() - block->timestamp )
    ( "bi", stats.get_txs_processed_before_block() )( "bo", stats.get_txs_accepted_before_block() )
    ( "ae", stats.get_txs_expired_after_block() )( "af", stats.get_txs_failed_after_block() )
    ( "aa", stats.get_txs_reapplied_after_block() )( "ap", stats.get_txs_postponed_after_block() )
    ( "ef", stats.get_creation_ts() - block->timestamp )
    ( "er", stats.get_wait_time() )( "ew", stats.get_work_time() )
    ( "eo", stats.get_cleanup_time() )( "ea", stats.get_total_time() )
  );
}

void block_data::notify_stats() const
{
  //TODO: supplement
}

void new_block_data::on_fork_db_insert()
{
  block_data::on_fork_db_insert();
  stats.on_end_work();
  ilog( "New block ${b} successfully produced.", ( "b", block->block_num() ) );
}

void new_block_data::on_end_of_apply_block()
{
  block_data::on_end_of_apply_block();
  if( prom )
    prom->set_value();
  // only now we can let the witness thread continue to potentially look if it has to produce new
  // block; if we set promise when we broadcast the block, the state would not yet reflect new
  // head block, so for witness plugin it would look like it should produce the same block again
}

void new_block_data::on_failure( const fc::exception& e )
{
  block_data::on_failure( e );
  if( prom )
    prom->set_value();
  ilog( "New block ${b} failed during production with ${e}. Requested at ${r}, picked at ${p}",
    ( "b", block->block_num() )( "e", e )( "r", stats.get_creation_ts() - block_ts )
    ( "p", stats.get_start_ts() - block_ts ) );
}

void inc_block_data::on_fork_db_insert()
{
  block_data::on_fork_db_insert();
  // this is still before actual work starts
  // (but it might be end of work if block is on fork, which means it won't be applied)
}

void inc_block_data::on_end_of_apply_block()
{
  block_data::on_end_of_apply_block();
  if( prom )
    prom->set_value();
  stats.on_end_work();
  ilog( "Block ${b} successfully applied.", ( "b", block->block_num() ) );
}

void inc_block_data::on_failure( const fc::exception& e )
{
  block_data::on_failure( e );
  if( prom )
    prom->set_value();
  ilog( "Block ${b} failed during application with ${e}.", ( "b", block->block_num() )( "e", e ) );
}

void old_block_data::on_fork_db_insert()
{
  block_data::on_fork_db_insert();
  // nothing to do
}

void old_block_data::on_end_of_apply_block()
{
  block_data::on_end_of_apply_block();
  stats.on_end_work();
  ilog( "Block ${b} successfully reapplied.", ( "b", block->block_num() ) );
}

void old_block_data::on_failure( const fc::exception& e )
{
  block_data::on_failure( e );
  ilog( "Block ${b} failed during reapplication with ${e}.", ( "b", block->block_num() )( "e", e ) );
}

} } // hive::chain

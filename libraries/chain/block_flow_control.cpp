#include <hive/chain/block_flow_control.hpp>

#include <hive/utilities/notifications.hpp>

namespace hive { namespace chain {

block_flow_control::report_type block_flow_control::auto_report_type = block_flow_control::report_type::FULL;
block_flow_control::report_output block_flow_control::auto_report_output = block_flow_control::report_output::ILOG;

void block_flow_control::set_auto_report( const std::string& _option_type, const std::string& _option_output )
{
  if( _option_type == "NONE" )
    auto_report_type = report_type::NONE;
  else if( _option_type == "MINIMAL" )
    auto_report_type = report_type::MINIMAL;
  else if( _option_type == "REGULAR" )
    auto_report_type = report_type::REGULAR;
  else if( _option_type == "FULL" )
    auto_report_type = report_type::FULL;
  else
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Unknown block stats report type" );

  if( _option_output == "NOTIFY" )
    auto_report_output = report_output::NOTIFY;
  else if( _option_output == "ILOG" )
    auto_report_output = report_output::ILOG;
  else if( _option_output == "DLOG" )
    auto_report_output = report_output::DLOG;
  else
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Unknown block stats report output" );
}

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
  stats.on_end_work();
  except = e.dynamic_copy_exception();
}

void block_flow_control::on_worker_done() const
{
  if( auto_report_type == report_type::NONE )
    return;

  fc::variant_object report = get_report(auto_report_type);
  switch (auto_report_output)
  {
  case report_output::NOTIFY:
    hive::notify("Block stats", "block_stats", report);
    break;
  case report_output::ILOG:
    ilog("Block stats:${report}", (report));
    break;
  default:
    dlog("Block stats:${report}", (report));
    if (fc::logger::get("default").is_enabled(fc::log_level::info))
      fc::logger::get("default").log(fc::log_message(FC_LOG_CONTEXT(info), 
                                                     "#${num} lib:${lib} ${type} ${bp} txs:${txs} size:${size} offset:${offset} "
                                                     "before:{inc:${inc} ok:${ok}} "
                                                     "after:{exp:${exp} fail:${fail} appl:${appl} post:${post}} "
                                                     "exec:{offset:${offset} pre:${pre} work:${work} post:${post} all:${all}} "
                                                     "id:${id} ts:${ts}", report));
    break;
  };
}

fc::variant_object block_flow_control::get_report( report_type rt ) const
{
  if( rt == report_type::NONE || full_block == nullptr )
    return fc::variant_object();

  const char* type = "";
  if( !finished() || except )
    type = "broken";
  else if( forked() )
    type = "forked";
  else if( ignored() )
    type = "ignored";
  else
    type = buffer_type();

  const auto& block_header = full_block->get_block_header();
  auto block_ts = block_header.timestamp;

  fc::variant_object_builder report;
  report
    ( "num", full_block->get_block_num() )
    ( "lib", stats.get_last_irreversible_block_num() )
    ( "type", type );
  if( rt != report_type::MINIMAL )
  {
    report
      ( "id", full_block->get_block_id() )
      ( "ts", block_ts )
      ( "bp", block_header.witness );
  }
  report
    ( "txs", full_block->get_full_transactions().size() )
    ( "size", full_block->get_uncompressed_block_size() )
    ( "offset", stats.get_ready_ts() - block_ts );
  if( rt != report_type::MINIMAL )
  {
    fc::variant_object_builder before;
    before
      ( "inc", stats.get_txs_processed_before_block() )
      ( "ok", stats.get_txs_accepted_before_block() );
    if( rt == report_type::FULL )
    {
      before
        ( "auth", stats.get_txs_with_failed_auth_before_block() )
        ( "rc", stats.get_txs_with_no_rc_before_block() );
    }
    report( "before", before.get() );
  }
  if( rt != report_type::MINIMAL )
  {
    fc::variant_object_builder after;
    after
      ( "exp", stats.get_txs_expired_after_block() )
      ( "fail", stats.get_txs_failed_after_block() )
      ( "appl", stats.get_txs_reapplied_after_block() )
      ( "post", stats.get_txs_postponed_after_block() );
    report( "after", after.get() );
  }
  if( rt != report_type::MINIMAL )
  {
    fc::variant_object_builder exec;
    exec
      ( "offset", stats.get_creation_ts() - block_ts )
      ( "pre", stats.get_wait_time() )
      ( "work", stats.get_work_time() )
      ( "post", stats.get_cleanup_time() )
      ( "all", stats.get_total_time() );
    report( "exec", exec.get() );
  }

  return report.get();
}

void generate_block_flow_control::on_fork_db_insert() const
{
  block_flow_control::on_fork_db_insert();
  stats.on_end_work();
}

void generate_block_flow_control::on_end_of_apply_block() const
{
  block_flow_control::on_end_of_apply_block();
  trigger_promise();
  // only now we can let the witness thread continue to potentially look if it has to produce new
  // block; if we set promise when we broadcast the block, the state would not yet reflect new
  // head block, so for witness plugin it would look like it should produce the same block again
}

void generate_block_flow_control::on_failure( const fc::exception& e ) const
{
  block_flow_control::on_failure( e );
  trigger_promise();
}

void p2p_block_flow_control::on_end_of_apply_block() const
{
  block_flow_control::on_end_of_apply_block();
  trigger_promise();
  stats.on_end_work();
}

void p2p_block_flow_control::on_failure( const fc::exception& e ) const
{
  block_flow_control::on_failure( e );
  trigger_promise();
}

void sync_block_flow_control::on_worker_done() const
{
  //do not generate report: many stats make no practical sense for sync blocks
  //and the excess logging seems to be slowing down sync
  //...with exception to last couple blocks of syncing
  if( full_block && ( fc::time_point::now() - full_block->get_block_header().timestamp ) < HIVE_UP_TO_DATE_MARGIN__BLOCK_STATS )
    block_flow_control::on_worker_done();
}

void existing_block_flow_control::on_end_of_apply_block() const
{
  block_flow_control::on_end_of_apply_block();
  stats.on_end_work();
}

} } // hive::chain

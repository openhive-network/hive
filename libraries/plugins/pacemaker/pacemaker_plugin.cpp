#include <hive/plugins/pacemaker/pacemaker_plugin.hpp>

#include <hive/chain/block_log.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

#define BLOCK_EMISSION_LOOP_SLEEP_TIME (100000)

namespace hive { namespace plugins { namespace pacemaker {

using namespace hive::chain;
using namespace hive::protocol;

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;

namespace detail {

class pacemaker_plugin_impl
{
public:
  pacemaker_plugin_impl( boost::asio::io_service& io, appbase::application& app ) :
    _timer( io ),
    _chain_plugin( app.get_plugin< hive::plugins::chain::chain_plugin >() ),
    _p2p_plugin( app.get_plugin<hive::plugins::p2p::p2p_plugin >() ),
    _db( _chain_plugin.db() ),
    _source( app ),
    theApp( app )
  {}

  void schedule_emission_loop();
  block_emission_condition block_emission_loop();
  block_emission_condition maybe_emit_block();

  fc::time_point block_time() const { return _next_block->get_block_header().timestamp; }
  fc::time_point emission_time() const { return block_time() + fc::microseconds( _min_offset ); }
  fc::time_point failure_time() const { return block_time() + fc::microseconds( _max_offset ); }
  fc::time_point catch_up_time() const { return block_time() + fc::microseconds( _catch_up_offset ); }

  boost::asio::deadline_timer      _timer;

  plugins::chain::chain_plugin&    _chain_plugin;
  plugins::p2p::p2p_plugin&        _p2p_plugin;
  chain::database&                 _db;
  block_log                        _source;

  int64_t                          _min_offset = 0;
  int64_t                          _max_offset = 0;
  int64_t                          _catch_up_offset = 0;

  std::shared_ptr<full_block_type> _next_block;

  appbase::application& theApp;
};

class pacemaker_emit_block_flow_control : public p2p_block_flow_control
{
public:
  using p2p_block_flow_control::p2p_block_flow_control;
  virtual ~pacemaker_emit_block_flow_control() = default;

private:
  virtual const char* buffer_type() const override { return "pacemaker"; }
};

class pacemaker_sync_block_flow_control final : public pacemaker_emit_block_flow_control
{
public:
  using pacemaker_emit_block_flow_control::pacemaker_emit_block_flow_control;
  virtual ~pacemaker_sync_block_flow_control() = default;

private:
  virtual const char* buffer_type() const override { return "pacemaker-sync"; }
};

void pacemaker_plugin_impl::schedule_emission_loop()
{
  // Sleep for 100ms, before checking the block emission
  fc::time_point now = fc::time_point::now();
  int64_t time_to_sleep = BLOCK_EMISSION_LOOP_SLEEP_TIME - ( now.time_since_epoch().count() % BLOCK_EMISSION_LOOP_SLEEP_TIME );
  if( time_to_sleep < 50000 ) // we must sleep for at least 50ms
    time_to_sleep += BLOCK_EMISSION_LOOP_SLEEP_TIME;

  _timer.expires_from_now( boost::posix_time::microseconds( time_to_sleep ) );
  _timer.async_wait( boost::bind( &pacemaker_plugin_impl::block_emission_loop, this ) );
}

block_emission_condition pacemaker_plugin_impl::block_emission_loop()
{
  block_emission_condition result;
  do
  {
    try
    {
      result = maybe_emit_block();
    }
    catch( const fc::canceled_exception& )
    {
      //We're trying to exit. Go ahead and let this one out.
      throw;
    }
    catch( const fc::exception& e )
    {
      elog( "Got exception while emitting block:\n${e}", ( "e", e.to_detail_string() ) );
      result = block_emission_condition::exception_emitting_block;
    }

    switch( result )
    {
      case block_emission_condition::normal:
        break;
      case block_emission_condition::lag:
        if( !_catch_up_offset )
          ilog( "Trying to catch up..." );
        break;
      case block_emission_condition::too_early:
        break;
      case block_emission_condition::too_late:
        if( _catch_up_offset )
        {
          elog( "Pacemaker exit condition met: failed to emit block because node is not catching up. Now: ${n}; Block ts: ${t}; Last time to emit: ${f}",
            ( "n", fc::time_point::now() )( "t", block_time() )( "f", catch_up_time() ) );
        }
        else
        {
          elog( "Pacemaker exit condition met: failed to emit block because max offset was exceeded. Now: ${n}; Block ts: ${t}; Last time to emit: ${f}",
            ( "n", fc::time_point::now() )( "t", block_time() )( "f", failure_time() ) );
        }
        theApp.generate_interrupt_request();
        break;
      case block_emission_condition::no_more_blocks:
        ilog( "Pacemaker exit condition met: no more blocks left in source." );
        theApp.generate_interrupt_request();
        break;
      case block_emission_condition::exception_emitting_block:
        elog( "Pacemaker exit condition met: emitted block turned out to be invalid" );
        theApp.generate_interrupt_request();
        break;
    }
  }
  while( result == block_emission_condition::lag );

  if( !theApp.is_interrupt_request() )
    schedule_emission_loop();
  else
    ilog( "exiting block_emission_loop" );
  return result;
}

block_emission_condition pacemaker_plugin_impl::maybe_emit_block()
{
  fc::time_point now = fc::time_point::now();
  fc::time_point max_time = _catch_up_offset ? catch_up_time() : failure_time();
  if( now >= max_time )
    return block_emission_condition::too_late;
  if( _catch_up_offset )
  {
    _catch_up_offset = ( now - block_time() ).count();
    if( _catch_up_offset <= _max_offset )
    {
      ilog( "Caught up - switching to live sync" );
      _catch_up_offset = 0;
    }
  }

  fc::time_point earliest_emission_time = emission_time();
  if( now < earliest_emission_time )
    return block_emission_condition::too_early;

  std::shared_ptr< pacemaker_emit_block_flow_control > block_ctrl;
  if( _catch_up_offset )
    block_ctrl = std::make_shared< pacemaker_sync_block_flow_control >( _next_block, database::skip_nothing );
  else
    block_ctrl = std::make_shared< pacemaker_emit_block_flow_control >( _next_block, database::skip_nothing );
  _chain_plugin.accept_block( block_ctrl, false );
  _p2p_plugin.broadcast_block( _next_block );

  _next_block = _source.read_block_by_num( _next_block->get_block_num() + 1 );
  if( _next_block.get() == nullptr )
    return block_emission_condition::no_more_blocks;
  else if( emission_time() <= fc::time_point::now() ) //check if we could emit next block already
    return block_emission_condition::lag;
  else
    return block_emission_condition::normal;
}

} // detail

pacemaker_plugin::pacemaker_plugin() {}
pacemaker_plugin::~pacemaker_plugin() {}

void pacemaker_plugin::set_program_options(
  boost::program_options::options_description& cli,
  boost::program_options::options_description& cfg )
{
  cfg.add_options()
    ( "pacemaker-source", bpo::value<string>(), "path to block_log file - source of block emissions" )
    ( "pacemaker-min-offset", bpo::value<int>()->default_value( -300 ), "minimum time of emission offset from block timestamp in milliseconds, default -300ms" )
    ( "pacemaker-max-offset", bpo::value<int>()->default_value( 20000 ), "maximum time of emission offset from block timestamp in milliseconds, default 20000ms (when exceeded, node will be stopped)" )
    ;
}

void pacemaker_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{ try {
#ifndef IS_TEST_NET
  FC_ASSERT( false, "pacemaker can only be used in testnet" );
#endif

  ilog( "Initializing pacemaker plugin" );
  my = std::make_unique< detail::pacemaker_plugin_impl >( get_app().get_io_service(), get_app() );

  FC_ASSERT( options.count( "pacemaker-source" ) == 1, "Missing or redundant option pacemaker-source - path pointing to source of block emissions" );
  bfs::path sp = options.at( "pacemaker-source" ).as<string>();
  fc::path sourcePath;
  if( sp.is_relative() )
    sourcePath = get_app().data_dir() / sp;
  else
    sourcePath = sp;
  my->_source.open( sourcePath, my->_chain_plugin.get_thread_pool(), true );

  my->_min_offset = options.at( "pacemaker-min-offset" ).as< int >() * 1000;
  my->_max_offset = options.at( "pacemaker-max-offset" ).as< int >() * 1000;
  FC_ASSERT( my->_max_offset > std::min( my->_min_offset, 0l ), "pacemaker-max-offset needs to be positive value greater than pacemaker-min-offset" );

  //allow up to 1/3 of the block interval for emitting blocks (2x a normal node)
  my->_chain_plugin.set_write_lock_hold_time( HIVE_BLOCK_INTERVAL * 1000 / 3 ); // units = milliseconds

} FC_LOG_AND_RETHROW() }

void pacemaker_plugin::plugin_startup()
{ try {
  ilog("pacemaker plugin: plugin_startup() begin" );

  if( !my->_chain_plugin.is_p2p_enabled() )
  {
    ilog( "Pacemaker plugin cannot work when P2P plugin is disabled..." );
    // it is because main writer loop is not working then
    my->theApp.generate_interrupt_request();
    return;
  }

  uint32_t nextBlock = my->_db.head_block_num() + 1;
  my->_next_block = my->_source.read_block_by_num( nextBlock );
  FC_ASSERT( my->_next_block.get() != nullptr, "No block #${n} in data source.", ( "n", nextBlock ) );

  auto now = fc::time_point::now();
  auto max_time = my->failure_time();
  auto block_time = my->block_time();
  if( now >= max_time )
  {
    // Already too late to emit first block - we need to activate catch up phase when each new block
    // has to be a bit closer to valid time, but it can exceed max_offset
    my->_catch_up_offset = ( now - block_time ).count() + my->_max_offset;
    ilog( "Already too late to emit block #${b} with timestamp ${t}; Current time: ${now}",
      ( "b", nextBlock )( "t", block_time )( now ) );
    ilog( "Entering catch up mode with initial offset of ${o} microseconds.",
      ( "o", my->_catch_up_offset ) );
  }
  else
  {
    ilog( "First block to emit is #${b} with timestamp ${t}; Current time: ${now}",
      ( "b", nextBlock )( "t", block_time )( now ) );
  }

  my->schedule_emission_loop();

  ilog("pacemaker plugin: plugin_startup() end");
} FC_CAPTURE_AND_RETHROW() }

void pacemaker_plugin::plugin_shutdown()
{
  try
  {
    my->_timer.cancel();
  }
  catch( fc::exception& e )
  {
    edump( ( e.to_detail_string() ) );
  }
}

} } } // hive::plugins::pacemaker


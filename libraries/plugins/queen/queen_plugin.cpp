
#include <hive/chain/hive_fwd.hpp>

#include <hive/utilities/signal.hpp>

#include <hive/plugins/queen/queen_plugin.hpp>
#include <hive/plugins/p2p/p2p_plugin.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/witness_objects.hpp>

#define DEFAULT_QUEEN_TARGET_BLOCK_SIZE 0 // max blocks allowed by witnesses
#define DEFAULT_QUEEN_TARGET_TX_COUNT 0 // unlimited transactions

namespace hive { namespace plugins { namespace queen {

using namespace hive::protocol;
using namespace hive::chain;

namespace detail {

class queen_plugin_impl
{
public:
  queen_plugin_impl( queen_plugin& _plugin, appbase::application& app )
  : _chain( app.get_plugin< hive::plugins::chain::chain_plugin >() ),
    _witness( app.get_plugin< hive::plugins::witness::witness_plugin >() ),
    _db( _chain.db() ),
    _self( _plugin ), theApp( app ) {}
  ~queen_plugin_impl() {}

  void on_post_apply_transaction( const transaction_notification& note );
  void on_finish_push_block( const block_notification& note );
  void on_fail_apply_block( const block_notification& note );

  void prepare_for_new_block();

  void print_stats();

  chain::chain_plugin&          _chain;
  witness::witness_plugin&      _witness;
  chain::database&              _db;
  queen_plugin&                 _self;
  appbase::application&         theApp;

  uint32_t                      _target_block_size = DEFAULT_QUEEN_TARGET_BLOCK_SIZE;
  uint32_t                      _target_tx_count = DEFAULT_QUEEN_TARGET_TX_COUNT;

  uint32_t                      remaining_block_size = 0;
  uint32_t                      remaining_tx_count = 0;

  uint32_t                      blocks_observed = 0;
  uint32_t                      blocks_produced = 0;
  uint32_t                      transactions_observed = 0;
  uint32_t                      min_full_block_cycle_num = 0;
  uint32_t                      max_full_block_cycle_num = 0;
  fc::time_point                end_of_last_block;

  fc::microseconds              total_production_time; // sum of .exec.work from block stats of produced blocks
  fc::microseconds              total_application_time; // sum of .exec.post from block stats of produced blocks
  fc::microseconds              total_time; // sum of .exec.all from block stats of produced blocks
  fc::microseconds              min_full_block_cycle_time = fc::microseconds::maximum();
  fc::microseconds              max_full_block_cycle_time;
  fc::microseconds              total_full_block_cycle_time;

  boost::signals2::connection   _post_apply_transaction_conn;
  boost::signals2::connection   _finish_push_block_conn;
  boost::signals2::connection   _fail_apply_block_conn;
};

class queen_generate_block_flow_control final : public generate_block_flow_control
{
public:
  queen_generate_block_flow_control( queen_plugin_impl& _this, const fc::time_point_sec _block_ts,
    const protocol::account_name_type& _wo, const fc::ecc::private_key& _key, uint32_t _skip )
    : generate_block_flow_control( _block_ts, _wo, _key, _skip ), plugin( _this )
  {}
  virtual ~queen_generate_block_flow_control() = default;

  virtual bool skip_transaction_reapplication() const { return true; }

  virtual void on_worker_done( appbase::application& app ) const override
  {
    stats.recalculate_times( get_block_timestamp() );
    generate_block_flow_control::on_worker_done( app );
    plugin.total_production_time += stats.get_work_time();
    plugin.total_application_time += stats.get_cleanup_time();
    plugin.total_time += stats.get_total_time();
  }

private:
  virtual const char* buffer_type() const override { return "queen"; }

  queen_plugin_impl& plugin;
};

void queen_plugin_impl::on_post_apply_transaction( const chain::transaction_notification& note )
{
  bool new_tx = _db.is_validating_one_tx();
  if( new_tx || _db.is_reapplying_one_tx() )
  {
    auto size = note.full_transaction->get_transaction_size();
    if( new_tx )
      ++transactions_observed;

    // we can't rely on change in _db._pending_tx_size because it will be updated after this call
    if( remaining_block_size > size && remaining_tx_count > 1 )
    {
      remaining_block_size -= size;
      --remaining_tx_count;
    }
    else
    {
      // produce block and put it in write queue (it will be placed in front of potential
      // transactions and there should be no other blocks to process)
      const auto& data = _witness.get_production_data();
      const auto generate_block_ctrl = std::make_shared< queen_generate_block_flow_control >( *this,
        data.next_slot_time, data.scheduled_witness, data.scheduled_private_key, chain::database::skip_nothing );
      _chain.queue_generate_block_request( generate_block_ctrl ); //note - this call won't wait for block to be produced
      ++blocks_produced;
    }
  }
}

void queen_plugin_impl::on_finish_push_block( const block_notification& note )
{
  ++blocks_observed;
  prepare_for_new_block();
  auto now = fc::time_point::now();
  if( end_of_last_block != fc::time_point() )
  {
    auto block_cycle_time = now - end_of_last_block;
    if( min_full_block_cycle_time > block_cycle_time )
    {
      min_full_block_cycle_time = block_cycle_time;
      min_full_block_cycle_num = note.block_num;
    }
    if( max_full_block_cycle_time < block_cycle_time )
    {
      max_full_block_cycle_time = block_cycle_time;
      max_full_block_cycle_num = note.block_num;
    }
    total_full_block_cycle_time += block_cycle_time;
  }
  end_of_last_block = now;
}

void queen_plugin_impl::on_fail_apply_block( const chain::block_notification& note )
{
  // we could mark if the block is our own and only kill in such situation, however for the time
  // being the only sources of blocks are QUEEN itself (those blocks must not fail) and debug
  // plugin (those blocks also should not fail since they are produced from the same transactions
  // as QUEEN would, only earlier)
  elog( "Failed to apply block with QUEEN active. Closing." );
  theApp.kill();
}

void queen_plugin_impl::prepare_for_new_block()
{
  // note: it is important that this is called after witness plugin updated production data
  if( _db.head_block_time() >= _witness.get_production_data().next_slot_time )
  {
    elog( "QUEEN: internal error - wrong order of calls. Closing." );
    theApp.kill();
    return;
  }

  // reset counters
  const auto& dgpo = _db.get_dynamic_global_properties();
  uint32_t max_block_size = dgpo.maximum_block_size - 256; // 256 taken from trx_size_limit check in database.cpp
  if( _target_block_size )
    remaining_block_size = std::min( _target_block_size, max_block_size );
  else
    remaining_block_size = max_block_size;
  if( _target_tx_count )
    remaining_tx_count = _target_tx_count;
  else
    remaining_tx_count = -1;

  // check for nearest block that we can produce
  int i = 0;
  for( ; i < HIVE_MAX_WITNESSES; ++i )
  {
    const auto& production_data = _witness.get_production_data();
    if( production_data.produce_in_next_slot )
      break;
    // can't produce in that slot, check why:
    switch( production_data.condition )
    {
    case witness::block_production_condition::no_private_key:
      dlog( "QUEEN: can't sign for ${w} that is next in schedule, skipping to next slot.",
        ( "w", production_data.scheduled_witness ) );
      _witness.update_production_data( production_data.next_slot_time + HIVE_BLOCK_INTERVAL );
      break;
    case witness::block_production_condition::low_participation:
      elog( "QUEEN: participation rate dropped below required level. Closing." );
      theApp.kill();
      break;
    default:
      // produced - how?! produce_in_next_slot is ( condition == produced )
      // not_synced - queen should enable production in witness plugin
      // not_my_turn - queen should enable queen_mode in witness plugin
      // not_time_yet - only used by disabled witness plugin production loop
      // lag - only used by disabled witness plugin production loop
      // wait_for_genesis - only used by disabled witness plugin production loop
      // exception_producing_block - only used by disabled witness plugin production loop
      elog( "QUEEN: internal error - impossible block condition detected (${c}). Closing.",
        ( "c", (int)production_data.condition ) );
      theApp.kill();
      break;
    }
  }

  if( i == HIVE_MAX_WITNESSES )
  {
    elog( "QUEEN: active schedule contains no witness that we can sign block for. Closing." );
    theApp.kill();
  }
}

void queen_plugin_impl::print_stats()
{
  if( _db._pending_tx_size > 0 )
    ilog( "QUEEN ended with ${s} bytes of unused pending transactions.", ( "s", _db._pending_tx_size ) );
  else
    ilog( "QUEEN ended cleanly." );
  ilog( "Production stats for QUEEN:" );
  ilog( "${t} new transactions encountered during work",
    ( "t", transactions_observed ) );
  ilog( "${p} blocks out of ${a} were produced by QUEEN. Stopped at block #${b}",
    ( "p", blocks_produced )( "a", blocks_observed )( "b", _db.head_block_num() ) );
  auto x = blocks_produced == 0 ? 1u : blocks_produced;
  ilog( "Blocks produced in average time of ${p}μs, applied in ${a}μs, average full processing time ${f}μs",
    ( "p", total_production_time.count() / x )
    ( "a", total_application_time.count() / x )
    ( "f", total_time.count() / x )
  );
  x = blocks_observed <= 1 ? 1u : blocks_observed - 1; // first block is not measured
  ilog( "Full block cycle: min = ${m}μs at #${mb}, avg = ${a}μs, max = ${x}μs at #${xb}",
    ( "m", min_full_block_cycle_time )( "mb", min_full_block_cycle_num )
    ( "a", total_full_block_cycle_time.count() / x )
    ( "x", max_full_block_cycle_time )( "xb", max_full_block_cycle_num )
  );
}

} // detail

queen_plugin::queen_plugin() {}

queen_plugin::~queen_plugin() {}

void queen_plugin::set_program_options(
  boost::program_options::options_description& cli,
  boost::program_options::options_description& cfg
  )
{
  // queen plugin uses configuration of witness plugin (witnesses, keys)
  cfg.add_options()
    ( "queen-block-size", bpo::value<uint32_t>()->default_value( DEFAULT_QUEEN_TARGET_BLOCK_SIZE ), "Size of blocks expected to be filled (or max allowed by witnesses). Default value 0 means max blocks." )
    ( "queen-tx-count", bpo::value<uint32_t>()->default_value( DEFAULT_QUEEN_TARGET_TX_COUNT ), "Number of transactions in block. Default value 0 means no limit." )
    ;
}

void queen_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  try
  {
    ilog( "Initializing queen plugin" );
#ifndef USE_ALTERNATE_CHAIN_ID
    FC_ASSERT( false, "Queen plugin cannot be used with mainnet, since it has to turn off p2p" );
#endif

    my = std::make_unique< detail::queen_plugin_impl >( *this, get_app() );

    my->_chain.disable_p2p( false ); // also disables witness_plugin
    my->_witness.enable_queen_mode();

    uint32_t max_size = options.at( "queen-block-size" ).as<uint32_t>();
    uint32_t max_tx = options.at( "queen-tx-count" ).as<uint32_t>();
    FC_ASSERT( max_size <= HIVE_MAX_BLOCK_SIZE - 256, "Queen mode block size cannot exceed ${s}",
      ( "s", HIVE_MAX_BLOCK_SIZE - 256 ) ); // 256 taken from trx_size_limit check in database.cpp
    my->_target_block_size = max_size;
    my->_target_tx_count = max_tx;

    my->_post_apply_transaction_conn = my->_db.add_post_apply_transaction_handler(
      [&]( const chain::transaction_notification& note ) { my->on_post_apply_transaction( note ); }, *this, 0 );
    my->_finish_push_block_conn = my->_db.add_finish_push_block_handler(
      [&]( const chain::block_notification& note ) { my->on_finish_push_block( note ); }, *this, 0 );
    my->_fail_apply_block_conn = my->_db.add_fail_apply_block_handler(
      [&]( const chain::block_notification& note ) { my->on_fail_apply_block( note ); }, *this, 0 );
  }
  FC_CAPTURE_AND_RETHROW()
}

void queen_plugin::plugin_startup()
{
  std::string txs;
  if( my->_target_tx_count == 0 )
    txs = "unlimited transactions";
  else if( my->_target_tx_count == 1 )
    txs = "single transaction";
  else
    txs = std::to_string( my->_target_tx_count ) + " transactions";
  if( my->_target_block_size == 0 )
    ilog( "QUEEN enabled targeting full blocks (max allowed by witnesses) and ${txs}", ( txs ) );
  else
    ilog( "QUEEN enabled targeting blocks of size ${s} and ${txs}", ( "s", my->_target_block_size )( txs ) );

  my->prepare_for_new_block();
}

void queen_plugin::plugin_shutdown()
{
  my->print_stats();

  hive::utilities::disconnect_signal( my->_post_apply_transaction_conn );
  hive::utilities::disconnect_signal( my->_finish_push_block_conn );
  hive::utilities::disconnect_signal( my->_fail_apply_block_conn );
}

} } } // hive::plugins::queen

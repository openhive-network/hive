
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/transaction_status/transaction_status_plugin.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/util/type_registrar_definition.hpp>
#include <hive/protocol/config.hpp>

#include <hive/utilities/signal.hpp>

#include <fc/io/json.hpp>

#define TRANSACTION_STATUS_BLOCK_DEPTH_KEY            "transaction-status-block-depth"
#define TRANSACTION_STATUS_DEFAULT_BLOCK_DEPTH        64000

/*
  *                                                 trackable
  *                          .-------------------------------------------------------.
  *                         |                           |                             |
  *   <- - - - - - - - - - [*] - - - - - - - - - - - - [*] - - - - - - - - - - - - - [*]
  *                        /                            |                              \
  *               actual block depth            nominal block depth                head block
  *
  * - Within the trackable range, if the transaction is found we will return the status
  *      If the transaction is not found and an expiration is provided we will return the expiration status
  *
  * - Nominal values are values provided by the user
  *
  * - Actual values are calculated and used to determine when tracking needs to begin
  *      see `plugin_initialize`
  */

namespace hive { namespace plugins { namespace transaction_status {

namespace detail {

class transaction_status_impl
{
public:
  transaction_status_impl( appbase::application& app ) :
    _chain_plugin( app.get_plugin< hive::plugins::chain::chain_plugin >() ),
    _db( _chain_plugin.db() ) {}
  virtual ~transaction_status_impl() {}

  void on_post_apply_transaction( const transaction_notification& note );
  void on_pre_apply_block( const block_notification& note );
  void on_post_apply_block( const block_notification& note );

  plugins::chain::chain_plugin& _chain_plugin;
  chain::database&              _db;
  uint32_t                      nominal_block_depth = 0;       //!< User provided block-depth
  uint32_t                      actual_block_depth = 0;        //!< Calculated block-depth
  fc::time_point_sec            estimated_starting_timestamp;
  bool                          tracking = false;
  boost::signals2::connection   post_apply_transaction_connection;
  boost::signals2::connection   pre_apply_block_connection;
  boost::signals2::connection   post_apply_block_connection;
  fc::time_point_sec            estimate_starting_timestamp();
};

void transaction_status_impl::on_post_apply_transaction( const transaction_notification& note )
{
  if ( tracking )
  {
    _db.create< transaction_status_object >( [&]( transaction_status_object& obj )
    {
      obj.transaction_id = note.transaction_id;
      // don't assign rc_cost because currently we can't be sure the RC signal is going to be handled prior to tx status signal
      // (ABW: can be changed once RC is moved to consensus code, even if it remains non-consensus)
    } );
  }
}

void transaction_status_impl::on_pre_apply_block( const block_notification& note )
{
  fc::time_point_sec block_timestamp = note.get_block_timestamp();
  if ( not tracking && estimated_starting_timestamp <= block_timestamp )
  {
    // Make sure we caught up with blockchain head fast enough.
    estimated_starting_timestamp = estimate_starting_timestamp();

    if ( estimated_starting_timestamp <= block_timestamp )
    {
      ilog( "Transaction status tracking activated at block ${bn}, timestamp ${bt}",
        ("bn", note.block_num)("bt", block_timestamp) );
      tracking = true;
    }
  }
}

void transaction_status_impl::on_post_apply_block( const block_notification& note )
{
  if ( tracking )
  {
    // Update all status objects with the transaction current block number
    for ( const auto& e : note.full_block->get_full_transactions() )
    {
      const auto& tx_status_obj = _db.get< transaction_status_object, by_trx_id >( e->get_transaction_id() );

      _db.modify( tx_status_obj, [&] ( transaction_status_object& obj )
      {
        obj.block_num = note.block_num;
        obj.rc_cost = e->get_rc_cost();
      } );
    }

    // Store status block object. Note that empty blocks are stored too.
    _db.create< transaction_status_block_object >( [&]( transaction_status_block_object& obj )
    {
      obj.block_num = note.block_num;
      obj.timestamp = note.get_block_timestamp();
    } );

    // Remove elements from the indices that are deemed too old for tracking
    if ( note.block_num > actual_block_depth )
    {
      uint32_t obsolete_block = note.block_num - actual_block_depth;
      const auto& idx = _db.get_index< transaction_status_index >().indices().get< by_block_num >();

      auto itr = idx.begin();
      while ( itr != idx.end() && itr->block_num <= obsolete_block )
      {
        _db.remove( *itr );
        itr = idx.begin();
      }

      const auto& bidx = _db.get_index< transaction_status_block_index >().indices().get< by_block_num >();
      auto bitr = bidx.begin();
      while ( bitr != bidx.end() && bitr->block_num <= obsolete_block )
      {
        _db.remove( *bitr );
        bitr = bidx.begin();
      }
    }
  }
}

fc::time_point_sec transaction_status_impl::estimate_starting_timestamp()
{
  // Let's see how far are we from live sync now.
  fc::microseconds time_gap = _chain_plugin.get_time_gap_to_live_sync( _db.head_block_time() );
  // In live sync current head block time is fine.
  if( time_gap.count() <= 0 )
  {
    dlog( "Estimating starting timestamp, in live sync." );
    return _db.head_block_time();
  }

  // Compare the time gap to actual block depth gap.
  fc::microseconds actual_block_depth_gap = fc::seconds( int64_t( actual_block_depth ) * HIVE_BLOCK_INTERVAL );
  // Within actual block depth head block time is fine too.
  if( time_gap <= actual_block_depth_gap )
  {
    dlog( "Estimating starting timestamp, within actual block depth (${tg} seconds vs ${abdg} seconds).",
          ( "tg", time_gap.to_seconds() )( "abdg", actual_block_depth_gap.to_seconds() ) );
    return _db.head_block_time();
  }

  // Count missing time and estimated timestamp.
  fc::microseconds missing_time_gap = time_gap - actual_block_depth_gap;
  dlog( "Estimating starting timestamp, (block) time to live sync is ${tg} seconds, (block) time to start tracking is ${mtg} seconds.",
        ("tg", time_gap.to_seconds())( "mtg", missing_time_gap.to_seconds() ) );
  return _db.head_block_time() + missing_time_gap;
}

} // detail

transaction_status_plugin::transaction_status_plugin() {}
transaction_status_plugin::~transaction_status_plugin() {}

void transaction_status_plugin::set_program_options( boost::program_options::options_description& cli, boost::program_options::options_description& cfg )
{
  cfg.add_options()
    ( TRANSACTION_STATUS_BLOCK_DEPTH_KEY,   boost::program_options::value<uint32_t>()->default_value( TRANSACTION_STATUS_DEFAULT_BLOCK_DEPTH ), "Defines the number of blocks from the head block that transaction statuses will be tracked." )
    ;
}

void transaction_status_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  try
  {
    ilog( "transaction_status: plugin_initialize() begin" );
    my = std::make_unique< detail::transaction_status_impl >( get_app() );

    fc::mutable_variant_object state_opts;

    if( options.count( TRANSACTION_STATUS_BLOCK_DEPTH_KEY ) )
    {
      my->nominal_block_depth = options.at( TRANSACTION_STATUS_BLOCK_DEPTH_KEY ).as< uint32_t >();
      state_opts[ TRANSACTION_STATUS_BLOCK_DEPTH_KEY ] = my->nominal_block_depth;
    }

    // We need to track 1 hour of blocks in addition to the depth the user would like us to track
    my->actual_block_depth = my->nominal_block_depth + ( HIVE_MAX_TIME_UNTIL_EXPIRATION / HIVE_BLOCK_INTERVAL );

    dlog( "transaction status initializing" );
    dlog( "  -> nominal block depth: ${block_depth}", ("block_depth", my->nominal_block_depth) );
    dlog( "  -> actual block depth: ${actual_block_depth}", ("actual_block_depth", my->actual_block_depth) );

    HIVE_ADD_PLUGIN_INDEX(my->_db, transaction_status_index);
    HIVE_ADD_PLUGIN_INDEX(my->_db, transaction_status_block_index);

    get_app().get_plugin< chain::chain_plugin >().report_state_options( name(), state_opts );

    my->post_apply_transaction_connection = my->_db.add_post_apply_transaction_handler( [&]( const transaction_notification& note ) { try { my->on_post_apply_transaction( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->pre_apply_block_connection = my->_db.add_pre_apply_block_handler( [&]( const block_notification& note ) { try { my->on_pre_apply_block( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->post_apply_block_connection = my->_db.add_post_apply_block_handler( [&]( const block_notification& note ) { try { my->on_post_apply_block( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );

    ilog( "transaction_status: plugin_initialize() end" );
  } FC_CAPTURE_AND_RETHROW()
}

void transaction_status_plugin::plugin_startup()
{
  try
  {
    ilog( "transaction_status: plugin_startup() begin" );
    
    my->estimated_starting_timestamp = my->estimate_starting_timestamp();
    
    ilog( "transaction_status: plugin_startup() end" );

  } FC_CAPTURE_AND_RETHROW()
}

void transaction_status_plugin::plugin_shutdown()
{
  hive::utilities::disconnect_signal( my->post_apply_transaction_connection );
  hive::utilities::disconnect_signal( my->pre_apply_block_connection );
  hive::utilities::disconnect_signal( my->post_apply_block_connection );
}

bool transaction_status_plugin::is_tracking()
{
  return my->tracking;
}

fc::time_point_sec transaction_status_plugin::get_earliest_tracked_block_timestamp()
{
  const auto& idx = my->_db.get_index< transaction_status_block_index >().indices().get< by_block_num >();
  auto itr = idx.begin();
  return itr == idx.end() ? fc::time_point_sec() : itr->timestamp;
}

fc::time_point_sec transaction_status_plugin::get_last_irreversible_block_timestamp()
{
  auto last_irreversible_block_num = my->_db.get_last_irreversible_block_num();
  const auto& bo = my->_db.find< transaction_status_block_object, by_block_num >( last_irreversible_block_num );
  return bo == nullptr ? fc::time_point_sec() : bo->timestamp;
}

} } } // hive::plugins::transaction_status

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::plugins::transaction_status::transaction_status_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::plugins::transaction_status::transaction_status_block_index)

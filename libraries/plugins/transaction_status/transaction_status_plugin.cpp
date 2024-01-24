
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/transaction_status/transaction_status_plugin.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/util/type_registrar_definition.hpp>
#include <hive/protocol/config.hpp>

#include <fc/io/json.hpp>

#define TRANSACTION_STATUS_BLOCK_DEPTH_KEY            "transaction-status-block-depth"
#define TRANSACTION_STATUS_DEFAULT_BLOCK_DEPTH        64000

/*
  *                             window of uncertainty              trackable
  *                          .-------------------------. .---------------------------.
  *                         |                           |                             |
  *   <- - - - - - - - - - [*] - - - - - - - - - - - - [*] - - - - - - - - - - - - - [*]
  *                        /                            |                              \
  *               actual block depth            nominal block depth                head block
  *
  * - Within the window of uncertainy, if the transaction is found we will return the status
  *      If the transaction is not found and an expiration is provided, we will return `too_old`
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
  transaction_status_impl( appbase::application& app )
    : _db( app.get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}
  virtual ~transaction_status_impl() {}

  void on_post_apply_transaction( const transaction_notification& note );
  void on_post_apply_block( const block_notification& note );

  chain::database&              _db;
  uint32_t                      nominal_block_depth = 0;       //!< User provided block-depth
  uint32_t                      actual_block_depth = 0;        //!< Calculated block-depth
  uint32_t                      estimated_starting_block = 0;
  bool                          tracking = false;
  boost::signals2::connection   post_apply_transaction_connection;
  boost::signals2::connection   post_apply_block_connection;
  bool                          state_is_valid();
  uint32_t                      estimate_starting_block();
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
  else if ( estimated_starting_block <= note.block_num )
  {
    // Make sure we caught up with blockchain head fast enough.
    estimated_starting_block = estimate_starting_block();

    if ( estimated_starting_block <= note.block_num )
    {
      ilog( "Transaction status tracking activated at block ${block_num}, statuses will be available after block ${estimated_trackable_block}",
        ("block_num", note.block_num)("estimated_trackable_block", estimated_starting_block + actual_block_depth - nominal_block_depth ) );
      tracking = true;
    }
  }
}

/**
  * Determine if the plugin state is valid.
  *
  * We determine validity by checking if tracked block numbers included in our state
  * fit within desired range.
  *
  * \return True if the transaction state is considered valid, otherwise false
  */
bool transaction_status_impl::state_is_valid()
{
  const auto& idx = _db.get_index< transaction_status_index >().indices().get< by_block_num >();
  auto itr = idx.begin();
  auto ritr = idx.rbegin();
  const auto& bidx = _db.get_index< transaction_status_block_index >().indices().get< by_block_num >();
  auto bitr = bidx.begin();
  auto britr = bidx.rbegin();

  // Check that transaction index includes only transactions from desired range
  // and that transaction blocks are included in their index too
  bool tx_lower_bound_is_valid = itr == idx.end() || 
    ( itr->block_num >= estimated_starting_block && 
      _db.find< transaction_status_block_object, by_block_num >( itr->block_num ) != nullptr );
  bool tx_upper_bound_is_valid = ritr == idx.rend() || 
    ( ritr->block_num <= _db.head_block_num() &&
      _db.find< transaction_status_block_object, by_block_num >( ritr->block_num ) != nullptr );

  // Check that block index includes only blocks from desired range.
  bool block_lower_bound_is_valid = bitr == bidx.end() || bitr->block_num >= estimated_starting_block;
  bool block_upper_bound_is_valid = britr == bidx.rend() || britr->block_num <= _db.head_block_num();

  return tx_lower_bound_is_valid && tx_upper_bound_is_valid &&
         block_lower_bound_is_valid && block_upper_bound_is_valid;
}

uint32_t transaction_status_impl::estimate_starting_block()
{
  int64_t calculated_head_num = 0;
#ifdef IS_TEST_NET
  calculated_head_num = HIVE_TRANSACTION_STATUS_TESTNET_CALCULATED_HEAD_NUM;
#else
  fc::time_point_sec head_block_time = _db.head_block_time();
  uint32_t head_block_num = _db.head_block_num();

  // Let's calculate what number should the blockchain head be right now.
  fc::time_point_sec now = fc::time_point::now();
  fc::microseconds time_gap = now - head_block_time;
  int64_t block_gap = time_gap.to_seconds() / HIVE_BLOCK_INTERVAL;
  FC_ASSERT( block_gap >= 0 );
  calculated_head_num = int64_t( head_block_num ) + block_gap;
#endif

  return std::max< int64_t >({ 0, calculated_head_num - int64_t( actual_block_depth ) });
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
    /*
    if ( !my->actual_track_after_block )
    {
      ilog( "Transaction status tracking activated" );
      my->tracking = true;
    }
    */
    HIVE_ADD_PLUGIN_INDEX(my->_db, transaction_status_index);
    HIVE_ADD_PLUGIN_INDEX(my->_db, transaction_status_block_index);

    get_app().get_plugin< chain::chain_plugin >().report_state_options( name(), state_opts );

    my->post_apply_transaction_connection = my->_db.add_post_apply_transaction_handler( [&]( const transaction_notification& note ) { try { my->on_post_apply_transaction( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->post_apply_block_connection = my->_db.add_post_apply_block_handler( [&]( const block_notification& note ) { try { my->on_post_apply_block( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );

    ilog( "transaction_status: plugin_initialize() end" );
  } FC_CAPTURE_AND_RETHROW()
}

void transaction_status_plugin::plugin_startup()
{
  try
  {
    ilog( "transaction_status: plugin_startup() begin" );
    
    my->estimated_starting_block = my->estimate_starting_block();
    
    if ( !my->state_is_valid() )
    {
      wlog( "The transaction status plugin state does not contain valid tracking information for the last ${num_blocks} blocks yet. Catching up...",
        ("num_blocks", my->nominal_block_depth) );
    }

    ilog( "transaction_status: plugin_startup() end" );

  } FC_CAPTURE_AND_RETHROW()
}

void transaction_status_plugin::plugin_shutdown()
{
  chain::util::disconnect_signal( my->post_apply_transaction_connection );
  chain::util::disconnect_signal( my->post_apply_block_connection );
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

#ifdef IS_TEST_NET

bool transaction_status_plugin::state_is_valid()
{
  return my->state_is_valid();
}

#endif

} } } // hive::plugins::transaction_status

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::plugins::transaction_status::transaction_status_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::plugins::transaction_status::transaction_status_block_index)

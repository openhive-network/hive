#include <hive/plugins/debug_node/debug_node_plugin.hpp>

#include <hive/plugins/witness/block_producer.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/database_exceptions.hpp>

#include <hive/chain/rc/rc_utility.hpp>

#include <fc/io/buffered_iostream.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>

//#include <fc/thread/future.hpp>
//#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>

#include <sstream>
#include <string>

namespace hive { namespace plugins { namespace debug_node {

namespace detail {

class debug_generate_block_flow_control final : public hive::chain::generate_block_flow_control
{
public:
  using hive::chain::generate_block_flow_control::generate_block_flow_control;
  debug_generate_block_flow_control( const fc::time_point_sec _block_ts, const protocol::account_name_type& _wo,
    const fc::ecc::private_key& _key, uint32_t _skip, bool _print_stats )
    : generate_block_flow_control( _block_ts, _wo, _key, _skip ), print_stats( _print_stats ) {}
  virtual ~debug_generate_block_flow_control() = default;

  virtual void on_worker_done( appbase::application& app ) const override
  {
    if( print_stats )
    {
      stats.recalculate_times( get_block_timestamp() );
      generate_block_flow_control::on_worker_done( app );
    }
  }

private:
  virtual const char* buffer_type() const override { return "debug"; }

  bool print_stats = true;
};

class debug_node_plugin_impl
{
  public:
    debug_node_plugin_impl( appbase::application& app );
    virtual ~debug_node_plugin_impl();

    plugins::chain::chain_plugin&             _chain_plugin;
    chain::database&                          _db;
    plugins::witness::witness_plugin*         _witness_plugin_ptr = nullptr;

    typedef std::vector< std::pair< protocol::transaction_id_type, bool> > current_debug_update_transactions;
    current_debug_update_transactions         _current_debug_update_txs;
    bool                                      _force_new_artificial_transaction_for_debug_update = true;

    boost::signals2::connection               _pre_apply_transaction_conn;
    boost::signals2::connection               _post_apply_block_conn;

    appbase::application&                     theApp;
};

debug_node_plugin_impl::debug_node_plugin_impl( appbase::application& app ) :
  _chain_plugin(app.get_plugin< hive::plugins::chain::chain_plugin >()),
  _db(_chain_plugin.db()), theApp(app)
  {}

debug_node_plugin_impl::~debug_node_plugin_impl() {}

}

debug_node_plugin::debug_node_plugin() {}
debug_node_plugin::~debug_node_plugin() {}

void debug_node_plugin::set_program_options(
  options_description& cli,
  options_description& cfg )
{
  cfg.add_options()
    ("debug-node-edit-script,e",
      boost::program_options::value< std::vector< std::string > >()->composing(),
        "Database edits to apply on startup (may specify multiple times)")
    ;
}

void debug_node_plugin::plugin_initialize( const variables_map& options )
{
  my = std::make_shared< detail::debug_node_plugin_impl >( get_app() );

  if( options.count( "debug-node-edit-script" ) > 0 )
  {
    _edit_scripts = options.at( "debug-node-edit-script" ).as< std::vector< std::string > >();
  }

  // connect needed signals
  my->_pre_apply_transaction_conn = my->_db.add_pre_apply_transaction_handler(
    [this](const chain::transaction_notification& note){ on_pre_apply_transaction(note); }, *this, 0 );
  my->_post_apply_block_conn = my->_db.add_post_apply_block_handler(
    [this](const chain::block_notification& note){ on_post_apply_block(note); }, *this, 0 );
}

void debug_node_plugin::plugin_startup()
{
  my->_witness_plugin_ptr = my->theApp.find_plugin< hive::plugins::witness::witness_plugin >();
  /*for( const std::string& fn : _edit_scripts )
  {
    std::shared_ptr< fc::ifstream > stream = std::make_shared< fc::ifstream >( fc::path(fn) );
    fc::buffered_istream bstream(stream);
    fc::variant v = fc::json::from_stream( bstream, fc::json::strict_parser );
    load_debug_updates( v.get_object() );
  }*/
}

chain::database& debug_node_plugin::database() { return my->_db; }

const protocol::transaction_id_type& debug_node_plugin::make_artificial_transaction_for_debug_update()
{
  static size_t idx = 0;

  if( !my->_force_new_artificial_transaction_for_debug_update )
  {
    FC_ASSERT( !my->_current_debug_update_txs.empty(), "Internal error" ); // flag is only cleared by this routine after adding tx
    FC_ASSERT( !my->_current_debug_update_txs.back().second, "Internal debug_update transaction already applied, while not fully configured" );
      // it will fail if debug_update tries to call another debug_update or if there was problem with applying previous block
      // that contained debug_update (f.e. because witness key was updated for witness that was supposed to produce the block)
    return my->_current_debug_update_txs.back().first; // reuse last existing transaction
  }

  FC_ASSERT( my->_current_debug_update_txs.size() < WITNESS_CUSTOM_OP_BLOCK_LIMIT, "Too many internal transactions." );
    // witness plugin has limit on number of custom ops per user per block; you can increase it in testnet configuration

  ++idx;
  std::string idx_str( std::to_string( idx ) );
  hive::protocol::custom_operation op;
  op.required_auths = { HIVE_TEMP_ACCOUNT }; // we are using 'temp' account because it has open authority, so it should work even in mainnet
  op.id = 0;
  op.data = std::vector<char>( idx_str.begin(), idx_str.end() );

  const auto& dgpo = database().get_dynamic_global_properties();
  protocol::signed_transaction tx;
  tx.set_reference_block( dgpo.head_block_id );
  tx.set_expiration( dgpo.time + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  tx.operations.push_back( op );

  const auto pack_type = hive::protocol::serialization_mode_controller::get_current_pack();
  hive::chain::full_transaction_ptr ftx = hive::chain::full_transaction_type::create_from_signed_transaction( tx, pack_type, false );
  ftx->sign_transaction( std::vector<fc::ecc::private_key>{}, database().get_chain_id(), pack_type );
  my->_chain_plugin.push_transaction( ftx, 0 );

  // if we have internal transaction(s) and are here it means there was regular transaction
  // between last debug_update() and this one while there were no blocks;
  // note that above push_transaction() always sets the force flag to true
  my->_force_new_artificial_transaction_for_debug_update = false;

  my->_current_debug_update_txs.emplace_back( ftx->get_transaction_id(), false );

  return my->_current_debug_update_txs.back().first;
}


/*
void debug_apply_update( chain::database& db, const fc::variant_object& vo, bool logging )
{
  static const uint8_t
    db_action_nil = 0,
    db_action_create = 1,
    db_action_write = 2,
    db_action_update = 3,
    db_action_delete = 4,
    db_action_set_hardfork = 5;
  if( logging ) wlog( "debug_apply_update:  ${o}", ("o", vo) );

  // "_action" : "create"   object must not exist, unspecified fields take defaults
  // "_action" : "write"    object may exist, is replaced entirely, unspecified fields take defaults
  // "_action" : "update"   object must exist, unspecified fields don't change
  // "_action" : "delete"   object must exist, will be deleted

  // if _action is unspecified:
  // - delete if object contains only ID field
  // - otherwise, write

  graphene::db2::generic_id oid;
  uint8_t action = db_action_nil;
  auto it_id = vo.find("id");
  FC_ASSERT( it_id != vo.end() );

  from_variant( it_id->value(), oid );
  action = ( vo.size() == 1 ) ? db_action_delete : db_action_write;

  from_variant( vo["id"], oid );
  if( vo.size() == 1 )
    action = db_action_delete;

  fc::mutable_variant_object mvo( vo );
  mvo( "id", oid );
  auto it_action = vo.find("_action" );
  if( it_action != vo.end() )
  {
    const std::string& str_action = it_action->value().get_string();
    if( str_action == "create" )
      action = db_action_create;
    else if( str_action == "write" )
      action = db_action_write;
    else if( str_action == "update" )
      action = db_action_update;
    else if( str_action == "delete" )
      action = db_action_delete;
    else if( str_action == "set_hardfork" )
      action = db_action_set_hardfork;
  }

  switch( action )
  {
    case db_action_create:

      idx.create( [&]( object& obj )
      {
        idx.object_from_variant( vo, obj );
      } );

      FC_ASSERT( false );
      break;
    case db_action_write:
      db.modify( db.get_object( oid ), [&]( graphene::db::object& obj )
      {
        idx.object_default( obj );
        idx.object_from_variant( vo, obj );
      } );
      FC_ASSERT( false );
      break;
    case db_action_update:
      db.modify_variant( oid, mvo );
      break;
    case db_action_delete:
      db.remove_object( oid );
      break;
    case db_action_set_hardfork:
      {
        uint32_t hardfork_id;
        from_variant( vo[ "hardfork_id" ], hardfork_id );
        db.set_hardfork( hardfork_id, false );
      }
      break;
    default:
      FC_ASSERT( false );
  }
}
*/

void debug_node_plugin::calculate_modifiers_according_to_new_price(const hive::protocol::price& new_price,
  const hive::protocol::HIVE_asset& total_hive, const hive::protocol::VEST_asset& total_vests,
  hive::protocol::HIVE_asset& hive_modifier, hive::protocol::VEST_asset& vest_modifier ) const
{
  FC_ASSERT(new_price.base.symbol == HIVE_SYMBOL);
  FC_ASSERT(new_price.quote.symbol == VESTS_SYMBOL);

  hive_modifier = hive::protocol::HIVE_asset( 0 );
  vest_modifier = hive::protocol::VEST_asset( 0 );

  auto alpha_x = new_price.quote.amount * total_hive.amount;
  auto alpha_y = new_price.base.amount * total_vests.amount;

  if (alpha_x >= alpha_y)
  {
    /// Means that alpha is >= 1, so we will be increasing vests pool
    fc::uint128_t a = total_vests.amount.value;
    a *= (alpha_x - alpha_y).value;
    a /= alpha_y.value;
    vest_modifier = hive::protocol::VEST_asset( fc::uint128_to_int64(a) );
  }
  else
  {
    /// Means that alpha is < 1, so we will be increasing Hive pool
    fc::uint128_t b = total_hive.amount.value;
    b *= (alpha_y - alpha_x).value;
    b /= alpha_x.value;
    hive_modifier = hive::protocol::HIVE_asset( fc::uint128_to_int64(b) );
  }
}

void debug_node_plugin::debug_set_vest_price( const hive::protocol::price& new_price,
  fc::optional<protocol::transaction_id_type> transaction_id )
{
  hive::protocol::VEST_asset vest_modifier;
  hive::protocol::HIVE_asset hive_modifier;

  const auto& dgpo = database().get_dynamic_global_properties();
  calculate_modifiers_according_to_new_price( new_price, dgpo.total_vesting_fund_hive, dgpo.total_vesting_shares, hive_modifier, vest_modifier );
  ilog( "vest_modifier=${vest_modifier}, hive_modifier=${hive_modifier}", ( vest_modifier ) ( hive_modifier ) );

  ilog( "Before modification: total_vesting_shares=${vest}, total_vesting_fund_hive=${hive}",
    ( "vest", dgpo.total_vesting_shares )( "hive", dgpo.total_vesting_fund_hive ) );

  debug_update( [ this, vest_modifier, hive_modifier ]( chain::database& db )
  {
    const hive::chain::account_object& miner_account = db.get_account( HIVE_INIT_MINER_NAME );
    auto _update_initminer = [ &db, &vest_modifier, &miner_account ]()
    {
      /// If we increased vests pool, we need to put them to initminer account to avoid validate_invariants failure 
      db.modify( miner_account, [ &vest_modifier ]( hive::chain::account_object& account )
      {
        account.vesting_shares += vest_modifier;
      } );
    };
    db.rc.update_rc_for_custom_action( _update_initminer, miner_account );

    db.modify( db.get_dynamic_global_properties(), [ &vest_modifier, &hive_modifier ]( hive::chain::dynamic_global_property_object& p )
    {
      p.total_vesting_shares += vest_modifier;
      p.total_vesting_fund_hive += hive_modifier;
      p.current_supply += hive_modifier;
      p.virtual_supply += hive_modifier;
    } );
  }, hive::chain::database::skip_nothing, transaction_id );

  ilog( "After modification: total_vesting_shares=${vest}, total_vesting_fund_hive=${hive}",
    ( "vest", dgpo.total_vesting_shares )( "hive", dgpo.total_vesting_fund_hive ) );
  ilog( "Final price=${p}", ( "p", hive::protocol::price( dgpo.total_vesting_fund_hive, dgpo.total_vesting_shares ) ) );
}

uint32_t debug_node_plugin::debug_generate_blocks( fc::optional<fc::ecc::private_key> debug_private_key,
  uint32_t count, uint32_t skip, uint32_t miss_blocks, bool immediate_generation )
{
  if( count == 0 )
    return 0;

  chain::public_key_type debug_public_key;
  const witness::witness_plugin::t_signing_keys* signing_keys = nullptr;
  if( debug_private_key.valid() )
    debug_public_key = debug_private_key->get_public_key();
  if( my->_witness_plugin_ptr )
    signing_keys = &my->_witness_plugin_ptr->get_signing_keys();
  if( !debug_private_key.valid() && ( signing_keys == nullptr || signing_keys->empty() ) )
  {
    elog( "Skipping generation because I don't know the private key" );
    FC_ASSERT( false, "Skipping generation because I don't know the private key" );
    return 0;
  }

  chain::database& db = database();

  uint32_t slot = miss_blocks+1, produced = 0;
  while( produced < count )
  {
    uint32_t new_slot = miss_blocks+1;
    std::string scheduled_witness_name = db.get_scheduled_witness( slot );
    fc::time_point_sec scheduled_time = db.get_slot_time( slot );
    const chain::witness_object& scheduled_witness = db.get_witness( scheduled_witness_name );
    chain::public_key_type scheduled_key = scheduled_witness.signing_key;
    witness::witness_plugin::t_signing_keys::const_iterator it;
    fc::ecc::private_key private_key;
    if( scheduled_key == debug_public_key )
    {
      private_key = *debug_private_key;
    }
    else if( signing_keys && ( ( it = signing_keys->find( scheduled_key ) ) != signing_keys->end() ) )
    {
      private_key = it->second;
    }
    else
    {
      elog( "Missing key for witness ${w}, stopping generation.", ( "w", scheduled_witness_name ) );
      FC_ASSERT( false, "Missing key for witness ${w}, stopping generation.", ( "w", scheduled_witness_name ) );
      break;
    }

    auto generate_block_ctrl = std::make_shared< detail::debug_generate_block_flow_control >(scheduled_time,
      scheduled_witness_name, private_key, skip, logging);

    if( immediate_generation ) // use this mode when called from debug node API (it takes write lock) - also in most unit tests
    {
      try
      {
        generate_block_ctrl->on_write_queue_pop( 0, 0, 0, 0 );
        witness::block_producer bp( my->_chain_plugin );
        bp.generate_block( generate_block_ctrl.get() );
      }
      catch( const fc::exception& e )
      {
        generate_block_ctrl->on_failure( e );
      }
      catch( ... )
      {
        generate_block_ctrl->on_failure( fc::unhandled_exception( FC_LOG_MESSAGE( warn,
          "Unexpected exception while generating block." ), std::current_exception() ) );
      }
      generate_block_ctrl->on_worker_done( my->theApp );
      generate_block_ctrl->rethrow_if_exception();
    }
    else // use this mode when not under write lock (includes witness_tests in unit tests)
    {
      my->_chain_plugin.push_generate_block_request(generate_block_ctrl);
    }

    ++produced;
    slot = new_slot;
  }

  return produced;
}

uint32_t debug_node_plugin::debug_generate_blocks_until(
  fc::optional<fc::ecc::private_key> debug_private_key,
  const fc::time_point_sec& head_block_time,
  bool generate_sparsely,
  uint32_t skip,
  bool immediate_generation
)
{
  chain::database& db = database();

  if( db.head_block_time() >= head_block_time )
    return 0;

  uint32_t new_blocks = 0;

  if( generate_sparsely )
  {
    new_blocks += debug_generate_blocks( debug_private_key, 1, skip, 0, immediate_generation );
    auto slots_to_miss = db.get_slot_at_time( head_block_time );
    if( slots_to_miss > 1 )
    {
      slots_to_miss--;
      new_blocks += debug_generate_blocks( debug_private_key, 1, skip, slots_to_miss, immediate_generation );
    }
  }
  else
  {
    while( db.head_block_time() < head_block_time )
      new_blocks += debug_generate_blocks( debug_private_key, 1, skip, 0, immediate_generation );
  }

  return new_blocks;
}

void debug_node_plugin::on_pre_apply_transaction( const chain::transaction_notification& note )
{
  try
  {
    chain::database& db = database();

    if( db.is_validating_one_tx() )
      my->_force_new_artificial_transaction_for_debug_update = true;

    auto it = _debug_updates.find( note.transaction_id );
    if( it != _debug_updates.end() )
    {
      for( const auto& update : it->second.callbacks )
        update(db);
      if( db.is_validating_block() && it->second.internal_tx )
      {
        // note: linear search - should be ok, since we are limited by witness on how many custom ops can be in block for single account
        for( auto& internal_tx : my->_current_debug_update_txs )
          if( note.transaction_id == internal_tx.first )
          {
            // mark internal transaction as successfully executed (on final application of block)
            internal_tx.second = true;
            break;
          }
      }
    }
  }
  catch (const fc::exception& e)
  {
    FC_THROW_EXCEPTION(chain::plugin_exception, "An fc error occured during applying debug updates: ${what}",("what", e.to_detail_string()));
  }
  catch (...)
  {
    const auto current_exception = std::current_exception();

    if (current_exception)
    {
      try
      {
        std::rethrow_exception(current_exception);
      }
      catch( const std::exception& e )
      {
         FC_THROW_EXCEPTION(chain::plugin_exception, "An std error occured during applying debug updates: ${what}",("what", e.what()));
      }
    }
    else
      FC_THROW_EXCEPTION(chain::plugin_exception, "An unknown error occured during applying debug updates.");
  }
}

void debug_node_plugin::on_post_apply_block( const chain::block_notification& note )
{ try {
  if( allow_throw_exception )
    HIVE_ASSERT( false, hive::chain::plugin_exception, "Artificial exception was thrown" );
  if( !my->_current_debug_update_txs.empty() )
  {
    for( const auto& internal_tx : my->_current_debug_update_txs )
      HIVE_ASSERT( internal_tx.second, hive::chain::plugin_exception, "Failed to apply debug_update transaction" );
      // example source of above failing: debug_update changing timestamp of head block so internal transaction
      // is treated as expired when block is produced
    my->_current_debug_update_txs.clear();
    my->_force_new_artificial_transaction_for_debug_update = true;
  }
} FC_LOG_AND_RETHROW() }

/*void debug_node_plugin::set_json_object_stream( const std::string& filename )
{
  if( _json_object_stream )
  {
    _json_object_stream->close();
    _json_object_stream.reset();
  }
  _json_object_stream = std::make_shared< std::ofstream >( filename );
}*/

/*void debug_node_plugin::flush_json_object_stream()
{
  if( _json_object_stream )
    _json_object_stream->flush();
}*/

/*void debug_node_plugin::save_debug_updates( fc::mutable_variant_object& target )
{
  for( const std::pair< chain::block_id_type, std::vector< fc::variant_object > >& update : _debug_updates )
  {
    fc::variant v;
    fc::to_variant( update.second, v );
    target.set( update.first.str(), v );
  }
}*/

/*void debug_node_plugin::load_debug_updates( const fc::variant_object& target )
{
  for( auto it=target.begin(); it != target.end(); ++it)
  {
    std::vector< fc::variant_object > o;
    fc::from_variant(it->value(), o);
    _debug_updates[ chain::block_id_type( it->key() ) ] = o;
  }
}*/

void debug_node_plugin::plugin_shutdown()
{
  chain::util::disconnect_signal( my->_pre_apply_transaction_conn );
  chain::util::disconnect_signal( my->_post_apply_block_conn );
  /*if( _json_object_stream )
  {
    _json_object_stream->close();
    _json_object_stream.reset();
  }*/
  return;
}

} } } // hive::plugins::debug_node

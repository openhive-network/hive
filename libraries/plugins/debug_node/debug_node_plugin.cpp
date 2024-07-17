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
class debug_node_plugin_impl
{
  public:
    debug_node_plugin_impl( appbase::application& app );
    virtual ~debug_node_plugin_impl();

    plugins::chain::chain_plugin&             _chain_plugin;
    chain::database&                          _db;

    typedef std::vector< std::pair< protocol::transaction_id_type, bool> > current_debug_update_transactions;
    current_debug_update_transactions         _current_debug_update_txs;
    bool                                      _force_new_artificial_transaction_for_debug_update = true;

    boost::signals2::connection               _pre_apply_transaction_conn;
    boost::signals2::connection               _post_apply_block_conn;
};

debug_node_plugin_impl::debug_node_plugin_impl( appbase::application& app ) :
  _chain_plugin(app.get_plugin< hive::plugins::chain::chain_plugin >()),
  _db(_chain_plugin.db())
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

uint32_t debug_node_plugin::debug_generate_blocks(const std::string& debug_key, uint32_t count, uint32_t skip, uint32_t miss_blocks, bool immediate_generation)
{
  debug_generate_blocks_args args;
  debug_generate_blocks_return ret;

  args.debug_key = debug_key;
  args.count = count;
  args.skip = skip;
  args.miss_blocks = miss_blocks;
  debug_generate_blocks( ret, args, immediate_generation );
  return ret.blocks;
}

void debug_node_plugin::debug_generate_blocks(debug_generate_blocks_return& ret, const debug_generate_blocks_args& args, bool immediate_generation)
{
  if( args.count == 0 )
  {
    ret.blocks = 0;
    return;
  }

  fc::optional<fc::ecc::private_key> debug_private_key;
  chain::public_key_type debug_public_key;
  if( args.debug_key != "" )
  {
    debug_private_key = fc::ecc::private_key::wif_to_key( args.debug_key );
    FC_ASSERT( debug_private_key.valid() );
    debug_public_key = debug_private_key->get_public_key();
  }
  else
  {
    if( logging ) elog( "Skipping generation because I don't know the private key");
    ret.blocks = 0;
    return;
  }

  chain::database& db = database();

  uint32_t slot = args.miss_blocks+1, produced = 0;
  while( produced < args.count )
  {
    uint32_t new_slot = args.miss_blocks+1;
    std::string scheduled_witness_name = db.get_scheduled_witness( slot );
    fc::time_point_sec scheduled_time = db.get_slot_time( slot );
    const chain::witness_object& scheduled_witness = db.get_witness( scheduled_witness_name );
    chain::public_key_type scheduled_key = scheduled_witness.signing_key;
    if( logging )
    {
      wlog( "slot: ${sl}   time: ${t}   scheduled key is: ${sk}   dbg key is: ${dk}",
        ("sk", scheduled_key)("dk", debug_public_key)("sl", slot)("t", scheduled_time) );
    }
    uint32_t skip = args.skip;
    if( scheduled_key != debug_public_key )
    {
      if( args.edit_if_needed )
      {
        if( logging ) wlog( "Missing key for witness ${w}, skipping witness signature.", ("w", scheduled_witness_name) );
        skip |= hive::chain::database::skip_witness_signature;
      }
      else
        break;
    }

    auto generate_block_ctrl = std::make_shared< hive::chain::generate_block_flow_control >(scheduled_time,
      scheduled_witness_name, *debug_private_key, skip);

    if( immediate_generation )
    {
      witness::block_producer bp( my->_chain_plugin );
      bp.generate_block(generate_block_ctrl.get());
    }
    else
    {
      my->_chain_plugin.generate_block(generate_block_ctrl);
    }

    ++produced;
    slot = new_slot;
  }

  ret.blocks = produced;
  return;
}

uint32_t debug_node_plugin::debug_generate_blocks_until(
  const std::string& debug_key,
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
    new_blocks += debug_generate_blocks( debug_key, 1, skip, 0, immediate_generation );
    auto slots_to_miss = db.get_slot_at_time( head_block_time );
    if( slots_to_miss > 1 )
    {
      slots_to_miss--;
      new_blocks += debug_generate_blocks( debug_key, 1, skip, slots_to_miss, immediate_generation );
    }
  }
  else
  {
    while( db.head_block_time() < head_block_time )
      new_blocks += debug_generate_blocks( debug_key, 1, hive::chain::database::skip_nothing, 0, immediate_generation );
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

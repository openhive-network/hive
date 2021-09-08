#include <hive/plugins/debug_node/debug_node_plugin.hpp>

#include <hive/plugins/witness/witness_plugin.hpp>
#include <hive/plugins/p2p/p2p_plugin.hpp>
#include <hive/plugins/witness/block_producer.hpp>
#include <hive/chain/witness_objects.hpp>

#include <fc/io/buffered_iostream.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>

#include <fc/thread/future.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <sstream>
#include <string>

namespace hive { namespace plugins { namespace debug_node {

namespace detail {
class debug_node_plugin_impl
{
  public:
    debug_node_plugin_impl();
    virtual ~debug_node_plugin_impl();

    chain::database&                          _db;
    boost::signals2::connection               _pre_apply_block_conn;
    boost::signals2::connection               _post_apply_block_conn;
    boost::signals2::connection               _debug_conn;
    void on_post_apply_block( const chain::block_notification& note );
    void on_pre_apply_block( const chain::block_notification& note );
    void on_debug( const chain::debug_notification& note );
    void push_debug_transaction(const hive::protocol::private_key_type& signee, const hive::protocol::debug_operation &op);

  private:

    static fc::time_point_sec get_time_null_value() { return fc::time_point_sec::min(); }
};

debug_node_plugin_impl::debug_node_plugin_impl() :
  _db( appbase::app().get_plugin< chain::chain_plugin >().db() ) {}
debug_node_plugin_impl::~debug_node_plugin_impl() {}

  void debug_node_plugin_impl::on_pre_apply_block( const chain::block_notification& note )
  {
#ifdef IS_TEST_NET
  // const auto& dgpo = _db.get_dynamic_global_properties();
  // if(dgpo.get_debug_properties().fast_forward_stop_point != get_time_null_value())
    // _db.modify(dgpo, [](auto& gpo){ gpo.get_debug_properties().is_fast_forward_state = true; });
#endif
  }

  void debug_node_plugin_impl::on_post_apply_block( const chain::block_notification& note )
  {
    try
    {
  #ifdef IS_TEST_NET
      if(_db.is_fast_forward_state())
      {
        if(note.block.timestamp >= _db.get_dynamic_global_properties().get_debug_properties().fast_forward_stop_point)
        {
          const fc::microseconds new_offset = note.block.timestamp - fc::time_point::now();
          ilog("switching off debug properties");
          this->_db.modify(this->_db.get_dynamic_global_properties(), [&](auto& gdpo){
            gdpo.get_debug_properties().fast_forward_stop_point = get_time_null_value();
            gdpo.get_debug_properties().block_time_offset = new_offset;
            gdpo.get_debug_properties().blocks_per_witness = 1;
            gdpo.get_debug_properties().start_from_aslot = std::numeric_limits<uint64_t>::max();
          });

          ilog("finished fast-forwarding at block with date ${bdate} with offset ${offset}",
              ("bdate", note.block.timestamp)
              ("offset", new_offset)
          );
        } else {
          ilog("increasing block_time_offset");
          this->_db.modify(this->_db.get_dynamic_global_properties(), [](auto& gdpo){
            gdpo.get_debug_properties().block_time_offset += fc::seconds(HIVE_BLOCK_INTERVAL);
          });
          // appbase::app().get_plugin<hive::plugins::p2p::p2p_plugin>().update_refresh_rate();
        }
      }
  #endif
    }
    FC_LOG_AND_RETHROW();
  }

  void debug_node_plugin_impl::on_debug(const chain::debug_notification &note)
  {
#ifdef IS_TEST_NET
    ilog("got `on_debug` signal");
    if(note.new_fast_forward != get_time_null_value())
    {
      ilog("updating debug properties");
      this->_db.modify(this->_db.get_dynamic_global_properties(), [&](auto& gdpo){
        gdpo.get_debug_properties().fast_forward_stop_point = note.new_fast_forward;
        gdpo.get_debug_properties().blocks_per_witness = note.blocks_per_witness;
        gdpo.get_debug_properties().start_from_aslot = note.start_from_aslot;
      });
      // delay_to_set = fc::milliseconds(1);
    }

#endif
  }

  void debug_node_plugin_impl::push_debug_transaction(
    const hive::protocol::private_key_type& invoker_key,
    const hive::protocol::debug_operation &op
  )
  {
    hive::protocol::signed_transaction tx;
    tx.expiration = this->_db.head_block_time() + fc::seconds(HIVE_MAX_TIME_UNTIL_EXPIRATION - 1);

    tx.operations.push_back( op );
    tx.sign(invoker_key, _db.get_chain_id(), fc::ecc::fc_canonical);

    ilog("pushing transaction with debug operation");
    // auto& chain = appbase::app().get_plugin<plugins::chain::chain_plugin>();
    // chain.accept_transaction(tx);
    _db.push_transaction(tx);
  }
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
    ("edit-script", boost::program_options::value< std::vector< std::string > >()->composing(),
        "Database edits to apply on startup (may specify multiple times). Deprecated in favor of debug-node-edit-script.")
    ;
}

void debug_node_plugin::plugin_initialize( const variables_map& options )
{
  my = std::make_shared< detail::debug_node_plugin_impl >();

  if( options.count( "debug-node-edit-script" ) > 0 )
  {
    _edit_scripts = options.at( "debug-node-edit-script" ).as< std::vector< std::string > >();
  }

  if( options.count("edit-script") > 0 )
  {
    wlog( "edit-scripts is deprecated in favor of debug-node-edit-script" );
    auto scripts = options.at( "edit-script" ).as< std::vector< std::string > >();
    _edit_scripts.insert( _edit_scripts.end(), scripts.begin(), scripts.end() );
  }


  my->_post_apply_block_conn = my->_db.add_post_apply_block_handler(
    [this](const chain::block_notification& note){ my->on_post_apply_block(note); }, *this, 0 );

  my->_pre_apply_block_conn = my->_db.add_pre_apply_block_handler(
    [this](const chain::block_notification& note){ my->on_pre_apply_block(note); }, *this, 0 );

  my->_debug_conn = my->_db.add_debug_handler(
    [this](const chain::debug_notification& note ) { my->on_debug(note); }, *this, 0);
}

void debug_node_plugin::plugin_startup()
{
}

chain::database& debug_node_plugin::database() { return my->_db; }

uint32_t debug_node_plugin::debug_generate_blocks(
  const std::string& debug_key,
  uint32_t count,
  uint32_t skip,
  uint32_t miss_blocks
)
{
  debug_generate_blocks_args args;
  debug_generate_blocks_return ret;

  args.debug_key = debug_key;
  args.count = count;
  args.skip = skip;
  args.miss_blocks = miss_blocks;
  debug_generate_blocks( ret, args );
  return ret.blocks;
}

void debug_node_plugin::debug_generate_blocks(
  debug_generate_blocks_return& ret,
  const debug_generate_blocks_args& args )
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
    debug_private_key = hive::utilities::wif_to_key( args.debug_key );
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
  witness::block_producer bp( db );
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
    if( scheduled_key != debug_public_key )
    {
      if( args.edit_if_needed )
      {
        if( logging ) wlog( "Modified key for witness ${w}", ("w", scheduled_witness_name) );
        debug_update( [=]( chain::database& db )
        {
          db.modify( db.get_witness( scheduled_witness_name ), [&]( chain::witness_object& w )
          {
            w.signing_key = debug_public_key;
          });
        }, args.skip );
      }
      else
        break;
    }

    bp.generate_block( scheduled_time, scheduled_witness_name, *debug_private_key, args.skip );
    ++produced;
    slot = new_slot;
  }

  ret.blocks = produced;
  return;
}

void debug_node_plugin::debug_generate_blocks_until( const fc::string& invoker, const fc::string& invoker_private_key, const fc::time_point_sec fast_forwarding_end_date, const size_t blocks_per_witness )
{
  FC_ASSERT( fast_forwarding_end_date > this->my->_db.head_block_time() );
  hive::protocol::debug_operation op{};

  op.invoker = invoker;
  op.untill = fast_forwarding_end_date;
  op.blocks_per_witness = blocks_per_witness;
  op.start_from_aslot = my->_db.get_dynamic_global_properties().current_aslot + my->_db.get_witness_schedule_object().num_scheduled_witnesses;

  my->push_debug_transaction( *hive::utilities::wif_to_key(invoker_private_key), op );
}

void debug_node_plugin::apply_debug_updates()
{
  chain::database& db = database();
  chain::block_id_type head_id = db.head_block_id();
  auto it = _debug_updates.find( head_id );
  if( it == _debug_updates.end() )
    return;
  for( auto& update : it->second )
    update( db );
}

void debug_node_plugin::plugin_shutdown()
{
  chain::util::disconnect_signal( my->_post_apply_block_conn );
  chain::util::disconnect_signal( my->_debug_conn );
  return;
}

} } } // hive::plugins::debug_node

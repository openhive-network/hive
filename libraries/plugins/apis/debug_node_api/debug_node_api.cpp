#include <hive/plugins/debug_node_api/debug_node_api_plugin.hpp>
#include <hive/plugins/debug_node_api/debug_node_api.hpp>

#include <fc/filesystem.hpp>
#include <fc/optional.hpp>
#include <fc/variant_object.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/witness_objects.hpp>

namespace hive { namespace plugins { namespace debug_node {

namespace detail {

class debug_node_api_impl
{
  public:
    debug_node_api_impl( appbase::application& app ) :
      _chain( app.get_plugin< chain::chain_plugin >() ),
      _db( _chain.db() ),
      _block_reader( _chain.block_reader() ),
      _debug_node( app.get_plugin< debug_node_plugin >() ),
      theApp( app ) {}

    DECLARE_API_IMPL(
      (debug_generate_blocks)
      (debug_generate_blocks_until)
      (debug_get_head_block)
      (debug_get_witness_schedule)
      (debug_get_future_witness_schedule)
      (debug_get_hardfork_property_object)
      (debug_set_hardfork)
      (debug_set_vest_price)
      (debug_has_hardfork)
      (debug_get_json_schema)
      (debug_throw_exception)
      (debug_fail_transaction)
    )

    fc::optional<fc::ecc::private_key> get_debug_private_key( const std::string& debug_key_str ) const;

    chain::chain_plugin&              _chain;
    chain::database&                  _db;
    const hive::chain::block_read_i&  _block_reader;
    debug_node::debug_node_plugin&    _debug_node;

    appbase::application&             theApp;
};

fc::optional<fc::ecc::private_key> debug_node_api_impl::get_debug_private_key( const std::string& debug_key_str ) const
{
  fc::optional<fc::ecc::private_key> debug_private_key;
  if( debug_key_str != "" )
  {
    debug_private_key = fc::ecc::private_key::wif_to_key( debug_key_str );
    FC_ASSERT( debug_private_key.valid() );
  }
  return debug_private_key;
}

DEFINE_API_IMPL( debug_node_api_impl, debug_generate_blocks )
{
  return { _debug_node.debug_generate_blocks( get_debug_private_key( args.debug_key ), args.count, args.skip,
    args.miss_blocks, true ) }; // We can't use chain_plugin-like generation because we need to be under lock and then chain plugin route does not work (because it has its own lock)
}

DEFINE_API_IMPL( debug_node_api_impl, debug_generate_blocks_until )
{
  return { _debug_node.debug_generate_blocks_until( get_debug_private_key( args.debug_key ), args.head_block_time, args.generate_sparsely,
    chain::database::skip_nothing, true ) }; // We can't use chain_plugin-like generation because we need to be under lock and then chain plugin route does not work (because it has its own lock)
}

DEFINE_API_IMPL( debug_node_api_impl, debug_get_head_block )
{
  return { _block_reader.get_block_by_number(_db.head_block_num())->get_block() };
}

DEFINE_API_IMPL( debug_node_api_impl, debug_get_witness_schedule )
{
  //ABW: routine provides access to raw value of the object, so its result will be a bit different than equivalent from condenser_api/database_api
  return debug_get_witness_schedule_return( _db.get_witness_schedule_object() );
}

DEFINE_API_IMPL( debug_node_api_impl, debug_get_future_witness_schedule )
{
  //ABW: routine provides access to raw value of the object
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_26 ), "Future schedule only become available after HF26" );
  return debug_get_witness_schedule_return( _db.get_future_witness_schedule_object() );
}

DEFINE_API_IMPL( debug_node_api_impl, debug_get_hardfork_property_object )
{
  return _db.get_hardfork_property_object();
}

DEFINE_API_IMPL( debug_node_api_impl, debug_set_hardfork )
{
  if( args.hardfork_id > HIVE_NUM_HARDFORKS )
    return {};

  _debug_node.debug_update( [=]( chain::database& db )
  {
    db.set_hardfork( args.hardfork_id, false );
  }, hive::chain::database::skip_nothing, args.hook_to_tx );

  return {};
}

DEFINE_API_IMPL( debug_node_api_impl, debug_has_hardfork )
{
  return { _db.has_hardfork( args.hardfork_id ) };
}


DEFINE_API_IMPL( debug_node_api_impl, debug_set_vest_price )
{
  _debug_node.debug_set_vest_price( args.vest_price, args.hook_to_tx );

  return {};
}

DEFINE_API_IMPL( debug_node_api_impl, debug_get_json_schema )
{
  return { _db.get_json_schema() };
}

DEFINE_API_IMPL( debug_node_api_impl, debug_throw_exception )
{
  _debug_node.allow_throw_exception = args.throw_exception;
  return {};
}

DEFINE_API_IMPL( debug_node_api_impl, debug_fail_transaction )
{
  // the transaction will fail during applying an already produced block
  _debug_node.debug_update(
      [args]( chain::database& db ){ FC_ASSERT(db.is_validating_block() == false, "Artificial exception was thrown for tx ${t}", ("t", args.tx_id ) ); }
    , hive::chain::database::skip_nothing
    , args.tx_id
  );

  return {};
}

} // detail

debug_node_api::debug_node_api( appbase::application& app): my( new detail::debug_node_api_impl( app ) )
{
  JSON_RPC_REGISTER_API( HIVE_DEBUG_NODE_API_PLUGIN_NAME );
}

debug_node_api::~debug_node_api() {}

DEFINE_WRITE_APIS( debug_node_api,
  (debug_generate_blocks)
  (debug_generate_blocks_until)
  (debug_set_hardfork)
  (debug_set_vest_price)
  (debug_fail_transaction)
)
DEFINE_READ_APIS( debug_node_api,
  (debug_get_head_block)
  (debug_get_witness_schedule)
  (debug_get_future_witness_schedule)
  (debug_get_hardfork_property_object)
  (debug_has_hardfork)
)
DEFINE_LOCKLESS_APIS( debug_node_api,
  (debug_get_json_schema) // the whole schema thing is (and pretty much always was) dead
  (debug_throw_exception) // might be lockless because it just sets flag to trigger exception on next on_post_apply_block
)

} } } // hive::plugins::debug_node

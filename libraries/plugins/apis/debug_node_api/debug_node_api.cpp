#include <hive/plugins/debug_node_api/debug_node_api_plugin.hpp>
#include <hive/plugins/debug_node_api/debug_node_api.hpp>

#include <fc/filesystem.hpp>
#include <fc/optional.hpp>
#include <fc/variant_object.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/witness_objects.hpp>

namespace hive { namespace plugins { namespace debug_node {

namespace detail {

class debug_node_api_impl
{
  public:
    debug_node_api_impl( appbase::application& app ) :
      _db( app.get_plugin< chain::chain_plugin >().db() ),
      _debug_node( app.get_plugin< debug_node_plugin >() ),
      theApp( app ) {}

    DECLARE_API_IMPL(
      (debug_push_blocks)
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
    )

    chain::full_database& _db;
    debug_node::debug_node_plugin& _debug_node;

    appbase::application& theApp;
};

DEFINE_API_IMPL( debug_node_api_impl, debug_push_blocks )
{
  auto& src_filename = args.src_filename;
  auto count = args.count;
  auto skip_validate_invariants = args.skip_validate_invariants;

  if( count == 0 )
    return { 0 };

  fc::path src_path = fc::path( src_filename );
  fc::path index_path = fc::path( src_filename + ".index" );
  if( fc::exists(src_path) && fc::exists(index_path) &&
    !fc::is_directory(src_path) && !fc::is_directory(index_path)
    )
  {
    ilog( "Loading ${n} from block_log ${fn}", ("n", count)("fn", src_filename) );
    idump( (src_filename)(count)(skip_validate_invariants) );
    chain::block_log log( theApp );
    log.open( src_path );
    uint32_t first_block = _db.head_block_num()+1;
    uint32_t skip_flags = chain::database::skip_nothing;
    if( skip_validate_invariants )
      skip_flags = skip_flags | chain::database::skip_validate_invariants;
    for( uint32_t i=0; i<count; i++ )
    {
      std::shared_ptr<hive::chain::full_block_type> full_block;
      try
      {
        full_block = log.read_block_by_num( first_block + i );
        if (!full_block)
          FC_THROW("Unable to read block ${block_num}", ("block_num", first_block + i));
      }
      catch( const fc::exception& e )
      {
        elog("Could not read block ${i} of ${count}", (i)(count));
        continue;
      }

      try
      {
        hive::chain::existing_block_flow_control block_ctrl( full_block );
        _db.push_block( block_ctrl, skip_flags );
      }
      catch (const fc::exception& e)
      {
        elog( "Got exception pushing block ${bn} : ${bid} (${i} of ${n})", ("bn", full_block->get_block_num())("bid", full_block->get_block_id())("i", i)("n", count) );
        elog( "Exception backtrace: ${bt}", ("bt", e.to_detail_string()) );
      }
    }
    ilog( "Completed loading block_database successfully" );
    return { count };
  }
  return { 0 };
}

DEFINE_API_IMPL( debug_node_api_impl, debug_generate_blocks )
{
  debug_generate_blocks_return ret;
  _debug_node.debug_generate_blocks( ret, args, true ); // We can't use chain_plugin like generation because we need to be under lock and then chain plugin route does not work (because it has its own lock)
  return ret;
}

DEFINE_API_IMPL( debug_node_api_impl, debug_generate_blocks_until )
{
  return { _debug_node.debug_generate_blocks_until( args.debug_key, args.head_block_time, args.generate_sparsely, chain::database::skip_nothing, true ) };
}

DEFINE_API_IMPL( debug_node_api_impl, debug_get_head_block )
{
  return { _db.fetch_block_by_number(_db.head_block_num())->get_block() }; 
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

} // detail

debug_node_api::debug_node_api( appbase::application& app): my( new detail::debug_node_api_impl( app ) )
{
  JSON_RPC_REGISTER_API( HIVE_DEBUG_NODE_API_PLUGIN_NAME );
}

debug_node_api::~debug_node_api() {}

DEFINE_WRITE_APIS( debug_node_api,
  (debug_push_blocks)
  (debug_generate_blocks)
  (debug_generate_blocks_until)
  (debug_set_hardfork)
  (debug_set_vest_price)
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

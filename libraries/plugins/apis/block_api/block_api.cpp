#include <hive/chain/hive_fwd.hpp>
#include <appbase/application.hpp>

#include <hive/plugins/block_api/block_api.hpp>
#include <hive/plugins/block_api/block_api_plugin.hpp>

#include <hive/protocol/get_config.hpp>

#include <chrono>

namespace hive { namespace plugins { namespace block_api {

class block_api_impl
{
  public:
    block_api_impl();
    ~block_api_impl();

    DECLARE_API_IMPL(
      (get_block_header)
      (get_block)
      (get_block_range)
    )

    chain::database& _db;
};

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Constructors                                                     //
//                                                                  //
//////////////////////////////////////////////////////////////////////

block_api::block_api()
  : my( new block_api_impl() )
{
  JSON_RPC_REGISTER_API( HIVE_BLOCK_API_PLUGIN_NAME );
}

block_api::~block_api() {}

block_api_impl::block_api_impl()
  : _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}

block_api_impl::~block_api_impl() {}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Blocks and transactions                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////
DEFINE_API_IMPL( block_api_impl, get_block_header )
{
  get_block_header_return result;
  if( args.block_num <= _db.head_block_num() )
  {
    std::tuple< optional<signed_block>, optional<block_id_type>, optional<public_key_type> > _fetched = _db.fetch_block_by_number_unlocked( args.block_num, false/*fetch_all*/ );
    optional<signed_block> block = std::get<0>( _fetched );
    if( block )
      result.header = *block;
  }
  return result;
}

DEFINE_API_IMPL( block_api_impl, get_block )
{
  get_block_return result;
  if( args.block_num <= _db.head_block_num() )
  {
    std::tuple< optional<signed_block>, optional<block_id_type>, optional<public_key_type> > _fetched = _db.fetch_block_by_number_unlocked( args.block_num, true/*fetch_all*/ );

    optional<signed_block> block          = std::get<0>( _fetched );
    optional<block_id_type> block_id      = std::get<1>( _fetched );
    optional<public_key_type> signing_key = std::get<2>( _fetched );

    if( block )
      result.block = api_signed_block_object( *block, block_id, signing_key );
  }
  return result;
}

DEFINE_API_IMPL( block_api_impl, get_block_range )
{
  get_block_range_return result;
  auto count = args.count;
  auto head = _db.head_block_num();
  if( args.starting_block_num > head )
    count = 0;
  else if( args.starting_block_num + count - 1 > head )
    count = head - args.starting_block_num + 1;
  if( count )
  {
    uint64_t _time_begin = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();

    std::vector< std::tuple< optional<signed_block>, optional<block_id_type>, optional<public_key_type> > > _items = _db.fetch_block_range_unlocked( args.starting_block_num, count );

    uint64_t _interval = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count() - _time_begin;
    ilog( "fetch time: ${time}[ms]", ("time", _interval) );

    uint64_t _time_begin2 = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();

    for( auto& item : _items )
    {
      optional<signed_block> block          = std::get<0>( item );
      optional<block_id_type> block_id      = std::get<1>( item );
      optional<public_key_type> signing_key = std::get<2>( item );

      if( block )
        result.blocks.push_back( api_signed_block_object( *block, block_id, signing_key ) );
    }

    uint64_t _interval2 = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count() - _time_begin2;
    ilog( "push time: ${time}[ms]", ("time", _interval2) );
  }
  return result;
}

DEFINE_LOCKLESS_APIS( block_api,
  (get_block_header)
  (get_block)
  (get_block_range)
)

} } } // hive::plugins::block_api

#include <hive/chain/hive_fwd.hpp>
#include <appbase/application.hpp>

#include <hive/plugins/block_api/block_api.hpp>
#include <hive/plugins/block_api/block_api_plugin.hpp>

#include <hive/protocol/get_config.hpp>

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
  optional<signed_block_header> header = _db.fetch_block_header_by_number( args.block_num );
  if (header)
    result.header = *header;
  return result;
}

DEFINE_API_IMPL( block_api_impl, get_block )
{
  get_block_return result;
  optional<signed_block> block = _db.fetch_block_by_number( args.block_num );
  if (block)
    result.block = *block;
  return result;
}

DEFINE_API_IMPL( block_api_impl, get_block_range )
{
  get_block_range_return result;
  auto count = args.count;
  auto head = _db.head_block_num_from_fork_db();
  if( args.starting_block_num > head )
    count = 0;
  else if( args.starting_block_num + count - 1 > head )
    count = head - args.starting_block_num + 1;
  if( count )
  {
    vector<signed_block> blocks = _db.fetch_block_range( args.starting_block_num, count );
    result.blocks.reserve(blocks.size());
    for (const signed_block& block : blocks)
      result.blocks.push_back(block);
  }
  return result;
}

DEFINE_LOCKLESS_APIS( block_api,
  (get_block_header)
  (get_block)
  (get_block_range)
)

} } } // hive::plugins::block_api

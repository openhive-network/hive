#include <hive/chain/hive_fwd.hpp>
#include <appbase/application.hpp>

#include <hive/plugins/block_api/block_api.hpp>
#include <hive/plugins/block_api/block_api_plugin.hpp>

#include <hive/protocol/get_config.hpp>

namespace hive { namespace plugins { namespace block_api {

class block_api_impl
{
  public:
    block_api_impl( appbase::application& app );
    ~block_api_impl();

    DECLARE_API_IMPL(
      (get_block_header)
      (get_block)
      (get_block_range)
    )

    const hive::chain::block_read_i& _block_reader;
};

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Constructors                                                     //
//                                                                  //
//////////////////////////////////////////////////////////////////////

block_api::block_api( appbase::application& app )
  : my( new block_api_impl( app ) )
{
  JSON_RPC_REGISTER_API( HIVE_BLOCK_API_PLUGIN_NAME );
}

block_api::~block_api() {}

block_api_impl::block_api_impl( appbase::application& app )
  : _block_reader( app.get_plugin< hive::plugins::chain::chain_plugin >().block_reader() ) {}

block_api_impl::~block_api_impl() {}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Blocks and transactions                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////
DEFINE_API_IMPL( block_api_impl, get_block_header )
{
  get_block_header_return result;
  std::shared_ptr<full_block_type> block = 
    _block_reader.get_block_by_number(args.block_num, fc::seconds(1) );
  if( block )
    result.header = block->get_block_header();
  return result;
}

DEFINE_API_IMPL( block_api_impl, get_block )
{
  get_block_return result;
  std::shared_ptr<full_block_type> full_block = 
    _block_reader.get_block_by_number(args.block_num, fc::seconds(1));
  if( full_block )
    result.block = full_block;
  return result;
}

DEFINE_API_IMPL( block_api_impl, get_block_range )
{
  get_block_range_return result;
  auto count = args.count;
  auto head = _block_reader.head_block_num(fc::seconds(1));
  if( args.starting_block_num > head )
    count = 0;
  else if( args.starting_block_num + count - 1 > head )
    count = head - args.starting_block_num + 1;
  if( count )
  {
    std::vector<std::shared_ptr<full_block_type>> full_blocks = 
      _block_reader.fetch_block_range(args.starting_block_num, count, fc::seconds(1));
    result.blocks.reserve(full_blocks.size());
    for (const std::shared_ptr<full_block_type>& full_block : full_blocks)
      result.blocks.push_back(full_block);
  }
  return result;
}

DEFINE_LOCKLESS_APIS( block_api,
  (get_block_header)
  (get_block)
  (get_block_range)
)

} } } // hive::plugins::block_api

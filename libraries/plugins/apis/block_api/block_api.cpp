#include <hive/chain/steem_fwd.hpp>
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
   auto block = _db.fetch_block_by_number( args.block_num );

   if( block )
      result.header = *block;

   return result;
}

DEFINE_API_IMPL( block_api_impl, get_block )
{
   get_block_return result;
   auto block = _db.fetch_block_by_number( args.block_num );

   if( block )
      result.block = *block;

   return result;
}

DEFINE_READ_APIS( block_api,
   (get_block_header)
   (get_block)
)

} } } // hive::plugins::block_api

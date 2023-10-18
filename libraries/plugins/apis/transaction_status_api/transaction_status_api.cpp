#include <hive/plugins/transaction_status_api/transaction_status_api_plugin.hpp>
#include <hive/plugins/transaction_status_api/transaction_status_api.hpp>

#include <hive/plugins/transaction_status/transaction_status_objects.hpp>

namespace hive { namespace plugins { namespace transaction_status_api {

namespace detail {

class transaction_status_api_impl
{
public:
  transaction_status_api_impl( appbase::application& app ) :
    _db( app.get_plugin< hive::plugins::chain::chain_plugin >().db() ),
    _block_reader( app.get_plugin< chain::chain_plugin >().block_reader() ),
    _tsp( app.get_plugin< hive::plugins::transaction_status::transaction_status_plugin >() ) {}

  DECLARE_API_IMPL( (find_transaction) )

  chain::database& _db;
  const hive::chain::block_read_i& _block_reader;
  transaction_status::transaction_status_plugin& _tsp;
};

DEFINE_API_IMPL( transaction_status_api_impl, find_transaction )
{
  uint32_t earliest_tracked_block_num = _tsp.earliest_tracked_block_num();

  // Have we begun tracking?
  if ( _db.head_block_num() >= earliest_tracked_block_num )
  {
    auto last_irreversible_block_num = _db.get_last_irreversible_block_num();
    auto tso = _db.find< transaction_status::transaction_status_object, transaction_status::by_trx_id >( args.transaction_id );

    // If we are actively tracking this transaction
    if ( tso != nullptr)
    {
      // If we're not within a block
      if ( !tso->block_num )
        return {
          .status = transaction_status::within_mempool
        };

      // If we're in an irreversible block
      if ( tso->block_num <= last_irreversible_block_num )
        return {
          .status = transaction_status::within_irreversible_block,
          .block_num = tso->block_num,
          .rc_cost = tso->rc_cost
        };
      // We're in a reversible block
      else
        return {
          .status = transaction_status::within_reversible_block,
          .block_num = tso->block_num,
          .rc_cost = tso->rc_cost
        };
    }

    // If the user has provided us with an expiration
    if ( args.expiration.valid() )
    {
      const auto& expiration = *args.expiration;

      // Check if the expiration is before our earliest tracked block plus maximum transaction expiration
      std::shared_ptr<hive::chain::full_block_type> earliest_tracked_block = 
        _block_reader.fetch_block_by_number( earliest_tracked_block_num );
      if (expiration < earliest_tracked_block->get_block_header().timestamp + HIVE_MAX_TIME_UNTIL_EXPIRATION)
        return {
          .status = transaction_status::too_old
        };

      // If the expiration is on or before our last irreversible block
      if ( last_irreversible_block_num > 0 )
      {
        std::shared_ptr<hive::chain::full_block_type> last_irreversible_block = 
          _block_reader.fetch_block_by_number( last_irreversible_block_num );
        if (expiration <= last_irreversible_block->get_block_header().timestamp)
          return {
            .status = transaction_status::expired_irreversible
          };
      }
      // If our expiration is in the past
      if ( expiration < _db.head_block_time() )
        return {
          .status = transaction_status::expired_reversible
        };
    }
  }

  // Either the user did not provide an expiration or it is not expired and we didn't hear about this transaction
  return { .status = transaction_status::unknown };
}

} // hive::plugins::transaction_status_api::detail

transaction_status_api::transaction_status_api( appbase::application& app ) : my( std::make_unique< detail::transaction_status_api_impl >( app ) )
{
  JSON_RPC_REGISTER_API( HIVE_TRANSACTION_STATUS_API_PLUGIN_NAME );
}

transaction_status_api::~transaction_status_api() {}

DEFINE_READ_APIS( transaction_status_api, (find_transaction) )

} } } // hive::plugins::transaction_status_api

#include <hive/plugins/chain_api/chain_api_plugin.hpp>
#include <hive/plugins/chain_api/chain_api.hpp>

namespace hive { namespace plugins { namespace chain {

namespace detail {

class chain_api_impl
{
  public:
    chain_api_impl( appbase::application& app ) : _chain( app.get_plugin<chain_plugin>() ) {}

    DECLARE_API_IMPL(
      (push_transaction) )

  private:
    chain_plugin& _chain;
};

DEFINE_API_IMPL( chain_api_impl, push_transaction )
{
  push_transaction_return result;

  result.success = false;

  try
  {
    full_transaction_ptr dummy;
    _chain.determine_encoding_and_accept_transaction( dummy, args );
    result.success = true;
  }
  catch (const fc::exception& e)
  {
    result.error = e.to_detail_string();
  }
  catch (const std::exception& e)
  {
    result.error = e.what();
  }
  catch (...)
  {
    result.error = "unknown error";
  }

  return result;
}

} // detail

chain_api::chain_api( appbase::application& app ): my( new detail::chain_api_impl( app ) )
{
  JSON_RPC_REGISTER_API( HIVE_CHAIN_API_PLUGIN_NAME );
}

chain_api::~chain_api() {}

DEFINE_LOCKLESS_APIS( chain_api,
  (push_transaction)
)

} } } //hive::plugins::chain

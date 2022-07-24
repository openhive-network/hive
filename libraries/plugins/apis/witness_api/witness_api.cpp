#include <hive/plugins/witness_api/witness_api.hpp>
#include <hive/plugins/witness_api/witness_api_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>

#include <appbase/application.hpp>

namespace hive { namespace plugins { namespace witness_api {

namespace detail
{
  class witness_api_impl
  {
    public:
      witness_api_impl() : _witness(appbase::app().get_plugin<hive::plugins::witness::witness_plugin>())
      {}

      DECLARE_API_IMPL((enable_fast_confirm)
                       (disable_fast_confirm)
                       (is_fast_confirm_enabled))

      hive::plugins::witness::witness_plugin& _witness;
  };

  DEFINE_API_IMPL(witness_api_impl, enable_fast_confirm)
  {
    _witness.enable_fast_confirm();
    return void_type();
  }

  DEFINE_API_IMPL(witness_api_impl, disable_fast_confirm)
  {
    _witness.disable_fast_confirm();
    return void_type();
  }

  DEFINE_API_IMPL(witness_api_impl, is_fast_confirm_enabled)
  {
    return _witness.is_fast_confirm_enabled();
  }
} // detail

witness_api::witness_api() : my(new detail::witness_api_impl())
{
  JSON_RPC_REGISTER_API(HIVE_WITNESS_API_PLUGIN_NAME);
}

witness_api::~witness_api() {}

DEFINE_LOCKLESS_APIS(witness_api,
                     (enable_fast_confirm)
                     (disable_fast_confirm)
                     (is_fast_confirm_enabled))

} } } // hive::plugins::witness_api

#pragma once

#include <hive/plugins/rewards_api/rewards_api_args.hpp>
#include <hive/plugins/json_rpc/utility.hpp>

namespace appbase
{
  class application;
}

namespace hive { namespace plugins { namespace rewards_api {

namespace detail { class rewards_api_impl; }

class rewards_api
{
public:
  rewards_api( appbase::application& app );
  ~rewards_api();

  DECLARE_API(
    (simulate_curve_payouts)
  );
private:
  std::unique_ptr< detail::rewards_api_impl > my;
};

} } } //hive::plugins::rewards_api

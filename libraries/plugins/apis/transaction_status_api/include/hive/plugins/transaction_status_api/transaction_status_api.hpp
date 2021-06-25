#pragma once

#include <hive/plugins/transaction_status_api/transaction_status_api_args.hpp>
#include <hive/plugins/json_rpc/utility.hpp>

namespace hive { namespace plugins { namespace transaction_status_api {

namespace detail { class transaction_status_api_impl; }

class transaction_status_api
{
public:
  transaction_status_api();
  ~transaction_status_api();

  DECLARE_API_SIGNATURE( (find_transaction) )
private:
  std::unique_ptr< detail::transaction_status_api_impl > my;
};

} } } //hive::plugins::transaction_status_api


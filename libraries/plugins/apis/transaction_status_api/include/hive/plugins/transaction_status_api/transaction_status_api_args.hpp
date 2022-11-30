#pragma once

#include <hive/protocol/types.hpp>
#include <hive/protocol/transaction.hpp>
#include <hive/protocol/block_header.hpp>

#include <hive/plugins/json_rpc/utility.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>

namespace hive { namespace plugins { namespace transaction_status_api {

struct find_transaction_args
{
  chain::transaction_id_type transaction_id;
  fc::optional< fc::time_point_sec > expiration;
};

struct find_transaction_return
{
  transaction_status::transaction_status status;
  fc::optional< uint32_t > block_num;
  fc::optional< int64_t > rc_cost;
};

} } } // hive::transaction_status_api

FC_REFLECT( hive::plugins::transaction_status_api::find_transaction_args, (transaction_id)(expiration) )
FC_REFLECT( hive::plugins::transaction_status_api::find_transaction_return, (status)(block_num)(rc_cost) )

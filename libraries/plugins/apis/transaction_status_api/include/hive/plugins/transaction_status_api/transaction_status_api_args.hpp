#pragma once

#include <hive/protocol/types.hpp>
#include <hive/protocol/transaction.hpp>
#include <hive/protocol/block_header.hpp>

#include <hive/plugins/json_rpc/utility.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>

namespace hive { namespace plugins { namespace transaction_status_api {

template<template<typename T> typename optional_type>
struct find_transaction_args_base
{
  chain::transaction_id_type transaction_id;
  optional_type< fc::time_point_sec > expiration;
};

using find_transaction_args = find_transaction_args_base<fc::optional>;
using find_transaction_args_signature = find_transaction_args_base<fc::optional_init>;

template<template<typename T> typename optional_type>
struct find_transaction_return_base
{
  transaction_status::transaction_status status;
  optional_type< uint32_t > block_num;
};

using find_transaction_return = find_transaction_return_base<fc::optional>;
using find_transaction_return_signature = find_transaction_return_base<fc::optional_init>;

} } } // hive::transaction_status_api

FC_REFLECT( hive::plugins::transaction_status_api::find_transaction_args, (transaction_id)(expiration) )
FC_REFLECT( hive::plugins::transaction_status_api::find_transaction_args_signature, (transaction_id)(expiration) )

FC_REFLECT( hive::plugins::transaction_status_api::find_transaction_return, (status)(block_num) )
FC_REFLECT( hive::plugins::transaction_status_api::find_transaction_return_signature, (status)(block_num) )

#pragma once
#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/protocol/types.hpp>

#include <fc/optional.hpp>

namespace hive { namespace plugins { namespace chain {

namespace detail { class chain_api_impl; }

struct push_block_args
{
  hive::chain::signed_block block;
  bool                         currently_syncing = false;
};

using push_block_args_signature = push_block_args;

template<template<typename T> typename optional_type>
struct push_block_return_base
{
  bool              success;
  optional_type<string>  error;
};

using push_block_return = push_block_return_base<fc::optional>;
using push_block_return_signature = push_block_return_base<fc::optional_init>;

typedef hive::chain::signed_transaction push_transaction_args;
using push_transaction_args_signature = push_transaction_args;

template<template<typename T> typename optional_type>
struct push_transaction_return_base
{
  bool              success;
  optional_type<string>  error;
};

using push_transaction_return = push_transaction_return_base<fc::optional>;
using push_transaction_return_signature = push_transaction_return_base<fc::optional_init>;

class chain_api
{
  public:
    chain_api();
    ~chain_api();

    DECLARE_API_SIGNATURE(
      (push_block)
      (push_transaction) )
    
  private:
    std::unique_ptr< detail::chain_api_impl > my;
};

} } } // hive::plugins::chain

FC_REFLECT( hive::plugins::chain::push_block_args, (block)(currently_syncing) )

FC_REFLECT( hive::plugins::chain::push_block_return, (success)(error) )
FC_REFLECT( hive::plugins::chain::push_block_return_signature, (success)(error) )

FC_REFLECT( hive::plugins::chain::push_transaction_return, (success)(error) )
FC_REFLECT( hive::plugins::chain::push_transaction_return_signature, (success)(error) )

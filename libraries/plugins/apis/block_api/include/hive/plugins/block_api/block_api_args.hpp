#pragma once
#include <hive/plugins/block_api/block_api_objects.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/transaction.hpp>
#include <hive/protocol/block_header.hpp>

#include <hive/plugins/json_rpc/utility.hpp>

namespace hive { namespace plugins { namespace block_api {

/* get_block_header */

struct get_block_header_args
{
  uint32_t block_num;
};

using get_block_header_args_signature = get_block_header_args;

template<template<typename T> typename optional_type>
struct get_block_header_return_base
{
  optional_type< block_header > header;
};

using get_block_header_return = get_block_header_return_base<fc::optional>;
using get_block_header_return_signature = get_block_header_return_base<fc::optional_init>;

/* get_block */
struct get_block_args
{
  uint32_t block_num;
};

using get_block_args_signature = get_block_args;

template<template<typename T> typename optional_type>
struct get_block_return_base
{
  optional_type< api_signed_block_object > block;
};

using get_block_return = get_block_return_base<fc::optional>;
using get_block_return_signature = get_block_return_base<fc::optional_init>;

/* get_block_range */
struct get_block_range_args
{
  uint32_t starting_block_num;
  uint32_t count;
};

using get_block_range_args_signature = get_block_range_args;

struct get_block_range_return
{
  vector<api_signed_block_object> blocks;
};

using get_block_range_return_signature = get_block_range_return;

} } } // hive::block_api

FC_REFLECT( hive::plugins::block_api::get_block_header_args,
  (block_num) )

FC_REFLECT( hive::plugins::block_api::get_block_header_return,
  (header) )

FC_REFLECT( hive::plugins::block_api::get_block_header_return_signature,
  (header) )

FC_REFLECT( hive::plugins::block_api::get_block_args,
  (block_num) )

FC_REFLECT( hive::plugins::block_api::get_block_return,
  (block) )

FC_REFLECT( hive::plugins::block_api::get_block_return_signature,
  (block) )

FC_REFLECT( hive::plugins::block_api::get_block_range_args,
  (starting_block_num)
  (count) )

FC_REFLECT( hive::plugins::block_api::get_block_range_return,
  (blocks) )


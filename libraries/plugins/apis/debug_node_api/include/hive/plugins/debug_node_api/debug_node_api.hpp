#pragma once
#include <hive/plugins/json_rpc/utility.hpp>
#include <hive/plugins/database_api/database_api_objects.hpp>
#include <hive/plugins/debug_node/debug_node_plugin.hpp>

#include <hive/protocol/types.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>

namespace hive { namespace plugins { namespace debug_node {

using json_rpc::void_type;

namespace detail { class debug_node_api_impl; }

struct debug_push_blocks_args
{
  std::string                               src_filename;
  uint32_t                                  count;
  bool                                      skip_validate_invariants = false;
};

using debug_push_blocks_args_signature = debug_push_blocks_args;

struct debug_push_blocks_return
{
  uint32_t                                  blocks;
};

using debug_push_blocks_return_signature = debug_push_blocks_return;

struct debug_generate_blocks_until_args
{
  std::string                               debug_key;
  fc::time_point_sec                        head_block_time;
  bool                                      generate_sparsely = true;
};

using debug_generate_blocks_until_args_signature = debug_generate_blocks_until_args;

typedef debug_push_blocks_return debug_generate_blocks_until_return;
using debug_generate_blocks_until_return_signature = debug_generate_blocks_until_return;

typedef void_type debug_pop_block_args;
using debug_pop_block_args_signature = debug_pop_block_args;

template<template<typename T> typename optional_type>
struct debug_pop_block_return_base
{
  optional_type< protocol::signed_block > block;
};

using debug_pop_block_return = debug_pop_block_return_base<fc::optional>;
using debug_pop_block_return_signature = debug_pop_block_return_base<fc::optional_init>;

typedef void_type debug_get_witness_schedule_args;
using debug_get_witness_schedule_args_signature = debug_get_witness_schedule_args;

typedef database_api::api_witness_schedule_object debug_get_witness_schedule_return;
using debug_get_witness_schedule_return_signature = debug_get_witness_schedule_return;

typedef void_type debug_get_hardfork_property_object_args;
using debug_get_hardfork_property_object_args_signature = debug_get_hardfork_property_object_args;

typedef database_api::api_hardfork_property_object debug_get_hardfork_property_object_return;
using debug_get_hardfork_property_object_return_signature = debug_get_hardfork_property_object_return;

struct debug_set_hardfork_args
{
  uint32_t hardfork_id;
};

using debug_set_hardfork_args_signature = debug_set_hardfork_args;

typedef void_type debug_set_hardfork_return;
using debug_set_hardfork_return_signature = debug_set_hardfork_return;

typedef debug_set_hardfork_args debug_has_hardfork_args;
using debug_has_hardfork_args_signature = debug_has_hardfork_args;

struct debug_has_hardfork_return
{
  bool has_hardfork;
};

using debug_has_hardfork_return_signature = debug_has_hardfork_return;

typedef void_type debug_get_json_schema_args;
using debug_get_json_schema_args_signature = debug_get_json_schema_args;

struct debug_get_json_schema_return
{
  std::string schema;
};

using debug_get_json_schema_return_signature = debug_get_json_schema_return;

class debug_node_api
{
  public:
    debug_node_api();
    ~debug_node_api();

    DECLARE_API_SIGNATURE(
      /**
      * Push blocks from existing database.
      */
      (debug_push_blocks)

      /**
      * Generate blocks locally.
      */
      (debug_generate_blocks)

      /*
      * Generate blocks locally until a specified head block time. Can generate them sparsely.
      */
      (debug_generate_blocks_until)

      /*
      * Pop a block from the blockchain, returning it
      */
      (debug_pop_block)
      (debug_get_witness_schedule)
      (debug_get_hardfork_property_object)

      (debug_set_hardfork)
      (debug_has_hardfork)
      (debug_get_json_schema)
    )

  private:
    std::unique_ptr< detail::debug_node_api_impl > my;
};

} } } // hive::plugins::debug_node

FC_REFLECT( hive::plugins::debug_node::debug_push_blocks_args,
        (src_filename)(count)(skip_validate_invariants) )

FC_REFLECT( hive::plugins::debug_node::debug_push_blocks_return,
        (blocks) )

FC_REFLECT( hive::plugins::debug_node::debug_generate_blocks_until_args,
        (debug_key)(head_block_time)(generate_sparsely) )

FC_REFLECT( hive::plugins::debug_node::debug_pop_block_return,
        (block) )

FC_REFLECT( hive::plugins::debug_node::debug_pop_block_return_signature,
        (block) )

FC_REFLECT( hive::plugins::debug_node::debug_set_hardfork_args,
        (hardfork_id) )

FC_REFLECT( hive::plugins::debug_node::debug_has_hardfork_return,
        (has_hardfork) )

FC_REFLECT( hive::plugins::debug_node::debug_get_json_schema_return,
        (schema) )

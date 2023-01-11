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

struct debug_push_blocks_return
{
  uint32_t                                  blocks;
};

struct debug_generate_blocks_until_args
{
  std::string                               debug_key;
  fc::time_point_sec                        head_block_time;
  bool                                      generate_sparsely = true;
};

typedef debug_push_blocks_return debug_generate_blocks_until_return;

typedef void_type debug_pop_block_args;

struct debug_pop_block_return
{
  fc::optional< protocol::signed_block > block;
};

struct api_witness_schedule_object
{
  api_witness_schedule_object() {}
  api_witness_schedule_object( const hive::chain::witness_schedule_object& wso ) :
    id( wso.get_id() ),
    current_virtual_time( wso.current_virtual_time ),
    next_shuffle_block_num( wso.next_shuffle_block_num ),
    num_scheduled_witnesses( wso.num_scheduled_witnesses ),
    elected_weight( wso.elected_weight ),
    timeshare_weight( wso.timeshare_weight ),
    miner_weight( wso.miner_weight ),
    witness_pay_normalization_factor( wso.witness_pay_normalization_factor ),
    median_props( wso.median_props ),
    majority_version( wso.majority_version ),
    max_voted_witnesses( wso.max_voted_witnesses ),
    max_miner_witnesses( wso.max_miner_witnesses ),
    max_runner_witnesses( wso.max_runner_witnesses ),
    hardfork_required_witnesses( wso.hardfork_required_witnesses ),
    account_subsidy_rd( wso.account_subsidy_rd ),
    account_subsidy_witness_rd( wso.account_subsidy_witness_rd ),
    min_witness_account_subsidy_decay( wso.min_witness_account_subsidy_decay )
  {
    size_t n = wso.current_shuffled_witnesses.size();
    current_shuffled_witnesses.reserve( n );
    std::transform( wso.current_shuffled_witnesses.begin(), wso.current_shuffled_witnesses.end(),
      std::back_inserter( current_shuffled_witnesses ),
      []( const hive::protocol::account_name_type& s ) -> std::string { return s; } );
    // ^ fixed_string std::string operator used here.
  }

  hive::chain::witness_schedule_id_type id;

  fc::uint128                           current_virtual_time = 0;
  uint32_t                              next_shuffle_block_num;
  vector<string>                        current_shuffled_witnesses;
  uint8_t                               num_scheduled_witnesses;
  uint8_t                               elected_weight;
  uint8_t                               timeshare_weight;
  uint8_t                               miner_weight;
  uint32_t                              witness_pay_normalization_factor;
  hive::chain::chain_properties         median_props;
  hive::protocol::version               majority_version;

  uint8_t                               max_voted_witnesses;
  uint8_t                               max_miner_witnesses;
  uint8_t                               max_runner_witnesses;
  uint8_t                               hardfork_required_witnesses;

  hive::chain::util::rd_dynamics_params account_subsidy_rd;
  hive::chain::util::rd_dynamics_params account_subsidy_witness_rd;
  int64_t                               min_witness_account_subsidy_decay = 0;
};

typedef void_type debug_get_witness_schedule_args;
typedef api_witness_schedule_object debug_get_witness_schedule_return;

typedef void_type debug_get_future_witness_schedule_args;
typedef api_witness_schedule_object debug_get_future_witness_schedule_return;

typedef void_type debug_get_hardfork_property_object_args;
typedef database_api::api_hardfork_property_object debug_get_hardfork_property_object_return;

struct debug_set_hardfork_args
{
  uint32_t hardfork_id;
};

struct debug_set_vest_price_args
{
  hive::protocol::price vest_price;
};

typedef void_type debug_set_vest_price_return;

typedef void_type debug_set_hardfork_return;

typedef debug_set_hardfork_args debug_has_hardfork_args;

struct debug_has_hardfork_return
{
  bool has_hardfork;
};

typedef void_type debug_get_json_schema_args;

struct debug_get_json_schema_return
{
  std::string schema;
};

struct debug_throw_exception_args
{
  bool throw_exception = false;
};

typedef void_type debug_throw_exception_return;

class debug_node_api
{
  public:
    debug_node_api();
    ~debug_node_api();

    DECLARE_API(
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
      (debug_get_future_witness_schedule)
      (debug_get_hardfork_property_object)

      (debug_set_hardfork)
      (debug_has_hardfork)
      (debug_get_json_schema)
      (debug_throw_exception)
      (debug_set_vest_price)
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

FC_REFLECT( hive::plugins::debug_node::api_witness_schedule_object,
        (id)
        (current_virtual_time)
        (next_shuffle_block_num)
        (current_shuffled_witnesses)
        (num_scheduled_witnesses)
        (elected_weight)
        (timeshare_weight)
        (miner_weight)
        (witness_pay_normalization_factor)
        (median_props)
        (majority_version)
        (max_voted_witnesses)
        (max_miner_witnesses)
        (max_runner_witnesses)
        (hardfork_required_witnesses)
        (account_subsidy_rd)
        (account_subsidy_witness_rd)
        (min_witness_account_subsidy_decay) )

FC_REFLECT( hive::plugins::debug_node::debug_set_hardfork_args,
        (hardfork_id) )

FC_REFLECT( hive::plugins::debug_node::debug_set_vest_price_args,
        (vest_price) )

FC_REFLECT( hive::plugins::debug_node::debug_has_hardfork_return,
        (has_hardfork) )

FC_REFLECT( hive::plugins::debug_node::debug_get_json_schema_return,
        (schema) )

FC_REFLECT( hive::plugins::debug_node::debug_throw_exception_args,
        (throw_exception) )

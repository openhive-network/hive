#pragma once

#include <hive/protocol/types.hpp>
#include <hive/protocol/asset.hpp>

namespace hive { namespace protocol {


struct witness_properties_change_flags
{
  uint32_t account_creation_changed       : 1;
  uint32_t max_block_changed              : 1;
  uint32_t hbd_interest_changed           : 1;
  uint32_t account_subsidy_budget_changed : 1;
  uint32_t account_subsidy_decay_changed  : 1;
  uint32_t key_changed                    : 1;
  uint32_t hbd_exchange_changed           : 1;
  uint32_t url_changed                    : 1;
};


struct witness_set_properties_inputs
{
  asset             account_creation_fee;
  uint32_t          maximum_block_size;
  uint16_t          hbd_interest_rate;
  int32_t           account_subsidy_budget;
  uint32_t          account_subsidy_decay;
  public_key_type   signing_key;
  price             hbd_exchange_rate;
  time_point_sec    last_hbd_exchange_update;
  string            url;
};


using witness_set_properties_props_t = flat_map< string, vector< char > >;
using witness_set_properties_input_compose_t = fc::static_variant<asset, uint32_t, uint16_t, int32_t, public_key_type, price, time_point_sec, string>;
namespace{

  using post_set_witness_properties_function_t = std::function<void(
    const string&,
    const bool,
    const witness_set_properties_input_compose_t&
  )>;
}

void extract_set_witness_properties(
  const witness_set_properties_props_t& input,
  witness_properties_change_flags& flags,
  witness_set_properties_inputs& properties,
  const bool has_hardfork_24,
  const post_set_witness_properties_function_t post_callback = [](auto, auto, auto){}
);


} } // hive::protocol

FC_REFLECT_TYPENAME( hive::protocol::witness_set_properties_input_compose_t );

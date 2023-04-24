#pragma once 

namespace consensus_state_provider
{
  std::shared_ptr<hive::chain::full_block_type> from_variant_to_full_block_ptr(const fc::variant& v, int block_num_debug );
}

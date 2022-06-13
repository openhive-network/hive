#pragma once

#include <fc/time.hpp>

#include <hive/chain/database.hpp>

namespace hive { namespace plugins { namespace chain {
  
class abstract_block_producer {
public:
  virtual ~abstract_block_producer() = default;

  virtual std::shared_ptr<hive::chain::full_block_type> generate_block(fc::time_point_sec when,
                                                                       const hive::chain::account_name_type& witness_owner,
                                                                       const fc::ecc::private_key& block_signing_private_key,
                                                                       uint32_t skip = hive::chain::database::skip_nothing) = 0;
};

} } } // hive::plugins::chain

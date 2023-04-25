#pragma once

#include <hive/chain/util/rd_dynamics.hpp>

#include <fc/reflect/reflect.hpp>

namespace hive { namespace chain {

struct rc_price_curve_params
{
  uint64_t        coeff_a = 0;
  uint64_t        coeff_b = 0;
  uint8_t         shift = 0;
};

struct rc_resource_params
{
  util::rd_dynamics_params resource_dynamics_params;
  rc_price_curve_params    price_curve_params;
};

class resource_credits
{
  public:
    // calculates cost of resource given curve params, current pool level, how much was used and regen rate
    static int64_t compute_cost(
      const rc_price_curve_params& curve_params,
      int64_t current_pool,
      int64_t resource_count,
      int64_t rc_regen );

};

} } // hive::chain

FC_REFLECT( hive::chain::rc_price_curve_params, (coeff_a)(coeff_b)(shift) )
FC_REFLECT( hive::chain::rc_resource_params, (resource_dynamics_params)(price_curve_params) )

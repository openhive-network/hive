
#pragma once
#include <hive/chain/rc/rc_utility.hpp>

#include <hive/protocol/types.hpp>

#include <fc/time.hpp>

namespace hive { namespace chain {

struct rc_curve_gen_params
{
  uint64_t                inelasticity_threshold_num    = 1;
  uint64_t                inelasticity_threshold_denom  = 128;
  uint64_t                a_point_num                   = 37;
  uint64_t                a_point_denom                 = 1000;
  uint64_t                u_point_num                   = 1;
  uint64_t                u_point_denom                 = 2;

  void validate()const;
};

void generate_rc_curve_params(
  rc_price_curve_params& price_curve_params,
  const util::rd_dynamics_params& resource_dynamics_params,
  const rc_curve_gen_params& curve_gen_params
  );

} } // hive::chain

FC_REFLECT( hive::chain::rc_curve_gen_params,
  (inelasticity_threshold_num)
  (inelasticity_threshold_denom)
  (a_point_num)
  (a_point_denom)
  (u_point_num)
  (u_point_denom)
  )

#pragma once

#include <hive/chain/hive_object_types.hpp>

#include <hive/chain/util/rd_dynamics.hpp>

#include <fc/reflect/reflect.hpp>

namespace hive { namespace chain {

class account_object;
class database;
class remove_guard;

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
    resource_credits( database& _db ) : db( _db ) {}

    // calculates cost of resource given curve params, current pool level, how much was used and regen rate
    static int64_t compute_cost(
      const rc_price_curve_params& curve_params,
      int64_t current_pool,
      int64_t resource_count,
      int64_t rc_regen );

    // updates RC related data on account after change in RC delegation
    void update_account_after_rc_delegation(
      const account_object& account,
      uint32_t now,
      int64_t delta,
      bool regenerate_mana = false ) const;
    // updates RC related data on account after change in vesting
    void update_account_after_vest_change(
      const account_object& account,
      uint32_t now,
      bool _fill_new_mana = true,
      bool _check_for_rc_delegation_overflow = false ) const;

    // checks if account had excess RC delegations that failed to remove in single block and are still being removed
    bool has_expired_delegation( const account_object& account ) const;
    // processes all excess RC delegations for current block
    void handle_expired_delegations() const;

  private:
    // processes excess RC delegations of single delegator according to limits set by guard
    void remove_delegations(
      int64_t& delegation_overflow,
      account_id_type delegator_id,
      uint32_t now,
      remove_guard& obj_perf ) const;

    database& db;
};

} } // hive::chain

FC_REFLECT( hive::chain::rc_price_curve_params, (coeff_a)(coeff_b)(shift) )
FC_REFLECT( hive::chain::rc_resource_params, (resource_dynamics_params)(price_curve_params) )

#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/rc/resource_count.hpp>

#include <hive/chain/util/rd_dynamics.hpp>

#include <hive/protocol/optional_automated_actions.hpp>

#include <fc/reflect/reflect.hpp>

namespace hive { namespace chain {

class account_object;
class database;
class remove_guard;
struct rc_info;

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

    enum class report_type
    {
      NONE, //no report
      MINIMAL, //just basic stats - no operation or payer stats
      REGULAR, //no detailed operation stats
      FULL //everything
    };
    // builds JSON (variant) representation of given RC stats
    static fc::variant_object get_report( report_type rt, const rc_stats_object& stats );

    // scans transaction for used resources
    static void count_resources(
      const hive::protocol::signed_transaction& tx,
      const size_t size,
      count_resources_result& result,
      const fc::time_point_sec now );

    // scans optional automated action for used resources
    static void count_resources(
      const hive::protocol::optional_automated_action& action,
      const size_t size,
      count_resources_result& result,
      const fc::time_point_sec now );

    // scans single nonstandard operation for used resource (implemented for rc_custom_operation)
    template< typename OpType >
    static void count_resources(
      const OpType& op,
      count_resources_result& result,
      const fc::time_point_sec now );

    /** scans database for state related to given operation (implemented for operation, rc_custom_operationand optional_automated_action)
      * see comment in definition for more details
      * Note: only selected operations consuming significant state handle differential usage
      */
    template< typename OpType >
    bool prepare_differential_usage( const OpType& op, count_resources_result& result ) const;

    // calculates cost of resource given curve params, current pool level, how much was used and regen rate
    static int64_t compute_cost(
      const rc_price_curve_params& curve_params,
      int64_t current_pool,
      int64_t resource_count,
      int64_t rc_regen );

    // calculates cost of given resource consumption, applies resource units and adds detailed cost to buffer
    int64_t compute_cost( rc_info* usage_info ) const;

    // returns account that is RC payer for given transaction (first operation decides)
    static hive::protocol::account_name_type get_resource_user( const hive::protocol::signed_transaction& tx );
    // returns account that is RC payer for given optional automated action
    static hive::protocol::account_name_type get_resource_user( const hive::protocol::optional_automated_action& action );

    // regenerates RC mana on given account - must be called before any operation that changes max RC mana
    void regenerate_rc_mana( const account_object& account, uint32_t now ) const;

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

    // consumes RC mana from payer account (or throws exception if not enough), supplements buffer with payer RC mana
    void use_account_rcs( rc_info* tx_info, int64_t rc ) const;

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

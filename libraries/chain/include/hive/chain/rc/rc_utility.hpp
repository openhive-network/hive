#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/rc/rc_operations.hpp>
#include <hive/chain/rc/resource_count.hpp>

#include <hive/chain/util/rd_dynamics.hpp>

#include <fc/reflect/reflect.hpp>

namespace hive { namespace protocol {

struct signed_transaction;

} }

namespace hive { namespace chain {

class account_object;
class database;
template< typename CustomOperationType >
class generic_custom_operation_interpreter;
class remove_guard;
class witness_schedule_object;
struct full_transaction_type;
struct rc_transaction_info;

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
    enum class report_output { DLOG, ILOG, NOTIFY, BOTH };
    enum class report_type
    {
      NONE, //no report
      MINIMAL, //just basic stats - no operation or payer stats
      REGULAR, //no detailed operation stats
      FULL //everything
    };
    // parses given config options for type and output of RC stats reports and sets them
    static void set_auto_report( const std::string& _option_type, const std::string& _option_output );
    // sets type of automatic report for all RC objects (note: there is usually one, just like database)
    static void set_auto_report( report_type rt, report_output ro )
    {
      auto_report_type = rt;
      auto_report_output = ro;
    }
    // builds JSON (variant) representation of given RC stats
    static fc::variant_object get_report( report_type rt, const rc_stats_object& stats );

    // scans transaction for used resources
    static void count_resources(
      const hive::protocol::signed_transaction& tx,
      const size_t size,
      count_resources_result& result,
      const fc::time_point_sec now );

    // scans single nonstandard operation for used extra resources (implemented for rc_custom_operation)
    template< typename OpType >
    static void count_resources(
      const OpType& op,
      count_resources_result& result,
      const fc::time_point_sec now );
    // collects and stores extra resources used by nonstandard operation as (positive) differential usage (must be called before transaction usage is collected)
    void handle_custom_op_usage( const rc_custom_operation& op, const fc::time_point_sec now ) const;

    /** scans database for state related to given operation (implemented for operation and rc_custom_operation)
      * see comment in definition for more details
      * Note: only selected operations consuming significant state handle differential usage
      */
    template< typename OpType >
    bool prepare_differential_usage( const OpType& op, count_resources_result& result ) const;
    // prepares and stores (negative) differential usage data for given operation (must be called before operation is executed)
    template< typename OpType >
    void handle_differential_usage( const OpType& op ) const;
    // returns current state of differential usage (for testing/logging purposes)
    const resource_count_type& get_differential_usage() const;

    // calculates cost of resource given curve params, current pool level, how much was used and regen rate
    static int64_t compute_cost(
      const rc_price_curve_params& curve_params,
      int64_t current_pool,
      int64_t resource_count,
      int64_t rc_regen );

    // calculates cost of given resource consumption, applies resource units and adds detailed cost to buffer
    int64_t compute_cost( rc_transaction_info* usage_info ) const;

    // returns account that is RC payer for given transaction (first operation decides)
    static hive::protocol::account_name_type get_resource_user( const hive::protocol::signed_transaction& tx );

    // regenerates RC mana on given account - must be called before any operation that changes max RC mana
    void regenerate_rc_mana( const account_object& account, const fc::time_point_sec now ) const;

    // updates RC related data on account after change in RC delegation
    void update_account_after_rc_delegation(
      const account_object& account,
      const fc::time_point_sec now,
      int64_t delta,
      bool regenerate_mana = false ) const;
    // updates RC related data on account after change in vesting
    void update_account_after_vest_change(
      const account_object& account,
      const fc::time_point_sec now,
      bool _fill_new_mana = true,
      bool _check_for_rc_delegation_overflow = false ) const;

    // updates RC mana and other related data before and after custom code - used by debug plugin
    void update_rc_for_custom_action( std::function<void()>&& callback, const account_object& account ) const;

    // consumes RC mana from payer account (or throws exception if not enough), supplements buffer with payer RC mana
    void use_account_rcs( rc_transaction_info* tx_info, int64_t rc ) const;

    // checks if account had excess RC delegations that failed to remove in single block and are still being removed
    bool has_expired_delegation( const account_object& account ) const;
    // processes all excess RC delegations for current block
    void handle_expired_delegations() const;

    // resource_new_accounts pool is controlled by witnesses - this should be called after every change in witness schedule
    void set_pool_params( const witness_schedule_object& wso ) const;

  private:
    // processes excess RC delegations of single delegator according to limits set by guard
    void remove_delegations(
      int64_t& delegation_overflow,
      account_id_type delegator_id,
      const fc::time_point_sec now,
      remove_guard& obj_perf ) const;

    // generates RC stats report and rotates data
    void handle_auto_report( uint32_t block_num, int64_t global_regen, const rc_pool_object& rc_pool ) const;

    resource_credits( database& _db ) : db( _db ) {} //can only be used by database
    database& db;
    friend class database;

    void initialize_evaluators();

    //temporary
    void on_pre_apply_block() const;
    void on_pre_apply_block_impl() const;
    //temporary
    void on_post_apply_block() const;
    void on_post_apply_block_impl() const;
    //temporary
    void on_pre_apply_transaction() const;
    void on_pre_apply_transaction_impl() const;
    //temporary
    void on_post_apply_transaction( const full_transaction_type& full_tx ) const;
    void on_post_apply_transaction_impl(
      const full_transaction_type& full_tx,
      const protocol::signed_transaction& tx ) const;

    std::shared_ptr< generic_custom_operation_interpreter< rc_custom_operation > > _custom_operation_interpreter;

    static report_type auto_report_type; //type of automatic daily rc stats reports
    static report_output auto_report_output; //output of automatic daily rc stat reports
};

} } // hive::chain

FC_REFLECT( hive::chain::rc_price_curve_params, (coeff_a)(coeff_b)(shift) )
FC_REFLECT( hive::chain::rc_resource_params, (resource_dynamics_params)(price_curve_params) )

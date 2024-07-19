#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/rc/rc_export_objects.hpp>
#include <hive/chain/rc/resource_count.hpp>

#include <hive/protocol/hive_custom_operations.hpp>

#include <hive/chain/util/rd_dynamics.hpp>
#include <hive/chain/evaluator.hpp>

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
    // collects and stores extra resources used by nonstandard operation (must be called before transaction usage is collected)
    void handle_custom_op_usage( const hive::protocol::rc_custom_operation& op, const fc::time_point_sec now );

    /** scans database for state related to given operation and remembers it as a discount (implemented for operation and rc_custom_operation)
      * must be called before operation is executed
      * see comment in definition for more details
      * Note: only selected operations consuming significant state get a discount for the related state already present
      */
    template< typename OpType >
    void handle_operation_discount( const OpType& op );

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

    // checks if account had excess RC delegations that failed to remove in single block and are still being removed
    bool has_expired_delegation( const account_object& account ) const;

    // resource_new_accounts pool is controlled by witnesses - this should be called after every change in witness schedule
    void set_pool_params( const witness_schedule_object& wso ) const;

    // resets information for new transaction - should be called at the start of each transaction (fills payer and op)
    void reset_tx_info( const protocol::signed_transaction& tx );
    // returns information collected for current transaction
    const rc_transaction_info& get_tx_info() const { return tx_info; }

    // resets information for new block - should be called at the start of each block
    void reset_block_info();
    // returns information collected for current block
    const rc_block_info& get_block_info() const { return block_info; }

  private:
    // consumes RC mana from payer account (or throws exception if not enough), supplements tx_info with RC mana
    void use_account_rcs( int64_t rc );

    // registers evaluator for custom rc operations
    void initialize_evaluators();

    // processes all excess RC delegations for current block
    void handle_expired_delegations() const;
    // processes excess RC delegations of single delegator according to limits set by guard
    void remove_delegations(
      int64_t& delegation_overflow,
      account_id_type delegator_id,
      const fc::time_point_sec now,
      remove_guard& obj_perf ) const;

    /** Calculates resources used (on top of potential discount and extra costs of custom ops),
      * their cost and consumes RC from transaction payer.
      * Throws exception when payer does not have enough mana (only when is_in_control() for now).
      */
    void finalize_transaction( const full_transaction_type& full_tx );
    // adjusts pools, rotates buckets and RC stats
    void finalize_block() const;

    // generates RC stats report and rotates data
    void handle_auto_report( uint32_t block_num, int64_t global_regen, const rc_pool_object& rc_pool ) const;

    resource_credits( database& _db ) : db( _db ) {} //can only be used by database
    database& db;
    friend class database;

    // information collected for current transaction
    rc_transaction_info tx_info;
    // information collected for current block
    rc_block_info block_info;

    std::shared_ptr< generic_custom_operation_interpreter< hive::protocol::rc_custom_operation > > _custom_operation_interpreter;

    static report_type auto_report_type; //type of automatic daily rc stats reports
    static report_output auto_report_output; //output of automatic daily rc stat reports
};

  using namespace hive::protocol;
  HIVE_DEFINE_PLUGIN_EVALUATOR( void, rc_custom_operation, delegate_rc );

} } // hive::chain

FC_REFLECT( hive::chain::rc_price_curve_params, (coeff_a)(coeff_b)(shift) )
FC_REFLECT( hive::chain::rc_resource_params, (resource_dynamics_params)(price_curve_params) )

HIVE_DECLARE_OPERATION_TYPE( hive::protocol::rc_custom_operation )

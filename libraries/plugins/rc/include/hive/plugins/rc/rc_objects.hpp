#pragma once
#include <hive/chain/util/manabar.hpp>

#include <hive/plugins/rc/rc_config.hpp>
#include <hive/plugins/rc/rc_export_objects.hpp>
#include <hive/plugins/rc/rc_stats.hpp>
#include <hive/plugins/rc/rc_utility.hpp>
#include <hive/plugins/rc/resource_count.hpp>
#include <hive/chain/database.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/hive_object_types.hpp>

#include <fc/int_array.hpp>

namespace hive { namespace chain {
struct by_account;
class remove_guard;
} }

namespace hive { namespace plugins { namespace rc {

using namespace std;
using namespace hive::chain;
using hive::protocol::asset;

enum rc_object_types
{
  rc_resource_param_object_type     = ( HIVE_RC_SPACE_ID << 8 ),
  rc_pool_object_type               = ( HIVE_RC_SPACE_ID << 8 ) + 1,
  rc_account_object_type            = ( HIVE_RC_SPACE_ID << 8 ) + 2,
  rc_direct_delegation_object_type  = ( HIVE_RC_SPACE_ID << 8 ) + 3,
  rc_usage_bucket_object_type       = ( HIVE_RC_SPACE_ID << 8 ) + 4,
  rc_pending_data_type              = ( HIVE_RC_SPACE_ID << 8 ) + 5,
  rc_stats_object_type              = ( HIVE_RC_SPACE_ID << 8 ) + 6,
  rc_expired_delegation_object_type = ( HIVE_RC_SPACE_ID << 8 ) + 7
};

class rc_resource_param_object : public object< rc_resource_param_object_type, rc_resource_param_object >
{
  CHAINBASE_OBJECT( rc_resource_param_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_resource_param_object )

    fc::int_array< rc_resource_params, HIVE_RC_NUM_RESOURCE_TYPES >
                    resource_param_array;
};
typedef oid_ref< rc_resource_param_object > rc_resource_param_id_type;

/**
  * Represents pools of resources (singleton).
  * With production of every block the resources are consumed, but also new resources are added
  * according to their blockbudgets defined in rc_resource_param_object and they also decay at
  * a rate defined there.
  * Cooperates with set of rc_usage_bucket_objects to always hold sum of the daily usage.
  * Tracks how different resources are consumed relative to their blockbudgets to determine their
  * popularity. Note that resource_new_accounts is treated as luxury item - its share is always
  * 100% and it is excluded from share calculation of other resources.
  */
class rc_pool_object : public object< rc_pool_object_type, rc_pool_object >
{
  CHAINBASE_OBJECT( rc_pool_object );
  public:
    template< typename Allocator >
    rc_pool_object( allocator< Allocator > a, uint64_t _id,
      const rc_resource_param_object& params, const resource_count_type& initial_usage )
      : id( _id )
    {
      for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
      {
        pool_array[i] = params.resource_param_array[i].resource_dynamics_params.pool_eq;
        usage_in_window[i] = initial_usage[i];
      }
      recalculate_resource_weights( params );
    }

    //sets resource amount in selected pool
    void set_pool( int poolIdx, int64_t value )
    {
      pool_array[ poolIdx ] = value;
    }

    //sets new value of per-block budget - returns if there was any change in value
    bool set_budget( int poolIdx, int64_t value )
    {
      bool result = ( last_known_budget[ poolIdx ] != value );
      last_known_budget[ poolIdx ] = value;
      return result;
    }

    //accumulates usage statistics for given resource
    void add_usage( int poolIdx, int64_t resource_consumed )
    {
      usage_in_window[ poolIdx ] += resource_consumed;
    }
    //should be called once per block after usage statistics are accumulated
    void recalculate_resource_weights( const rc_resource_param_object& params )
    {
      sum_of_resource_weights = 0;
      for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
      {
        if( i == resource_new_accounts )
          continue;
        resource_weights[i] = usage_in_window[i] * HIVE_100_PERCENT
          / params.resource_param_array[i].resource_dynamics_params.budget_per_time_unit;
        if( resource_weights[i] == 0 )
          resource_weights[i] = 1; //minimum weight
        sum_of_resource_weights += resource_weights[i];
      }
      resource_weights[ resource_new_accounts ] = sum_of_resource_weights; //always 100%
    }

    //current content of resource pool
    const resource_count_type& get_pool() const { return pool_array; }
    //current content of resource pool for selected resource
    int64_t get_pool( int poolIdx ) const { return pool_array[ poolIdx ]; }
    //global usage statistics within last HIVE_RC_BUCKET_TIME_LENGTH*HIVE_RC_WINDOW_BUCKET_COUNT window
    const resource_count_type& get_usage() const { return usage_in_window; }
    //same as above but for selected resource
    int64_t get_usage( int poolIdx ) const { return usage_in_window[ poolIdx ]; }
    //calculated once per block from usage, represents dividend part of share of each resource in global rc inflation
    uint64_t get_weight( int poolIdx ) const { return resource_weights[ poolIdx ]; }
    //counterweight for resource weight - get_weight(i)/get_weight_divisor() consititutes share of resource in global rc inflation
    uint64_t get_weight_divisor() const { return sum_of_resource_weights; }

    //for logging purposes only!!! calculates share (in basis points) of resource in global rc inflation
    uint16_t count_share( int poolIdx ) const { return get_weight( poolIdx ) * HIVE_100_PERCENT / get_weight_divisor(); }

    //gives last known per-block budget
    const resource_count_type& get_last_known_budget() const { return last_known_budget; }

  private:
    resource_count_type pool_array;
    resource_count_type usage_in_window;
    resource_count_type last_known_budget;
    fc::int_array< uint64_t, HIVE_RC_NUM_RESOURCE_TYPES > resource_weights; //in basis points of respective block-budgets
    uint64_t sum_of_resource_weights = 1; //should never be zero

  CHAINBASE_UNPACK_CONSTRUCTOR( rc_pool_object );
};
typedef oid_ref< rc_pool_object > rc_pool_id_type;

/**
  * Collects statistics to generate daily report.
  * Second instance keeps data from previous day and is used for API calls.
  */
class rc_stats_object : public object< rc_stats_object_type, rc_stats_object >
{
  CHAINBASE_OBJECT( rc_stats_object );
  public:
    template< typename Allocator >
    rc_stats_object( allocator< Allocator > a, uint64_t _id, uint32_t _forced_id )
      : id( _forced_id ) {}

    //for copying stats from pending to archive and reseting pending - call on pending object (RC_PENDING_STATS_ID)
    void archive_and_reset_stats( rc_stats_object& archive, const rc_pool_object& pool_obj,
      uint32_t _block_num, int64_t _regen );
    //collects stats using given transaction data
    void add_stats( const rc_info& tx_info );

    //starting block for statistics
    uint32_t get_starting_block() const { return block_num; }
    //global regeneration rate at starting block
    int64_t get_global_regen() const { return regen; }
    //budget at starting block
    const resource_count_type& get_budget() const { return budget; }
    //resource pool values at starting block
    const resource_count_type& get_pool() const { return pool; }
    //popularity share at starting block
    const resource_share_type& get_share() const { return share; }
    //cumulative stats for selected operation (HIVE_RC_NUM_OPERATIONS for multiop transaction stats)
    const rc_op_stats& get_op_stats( int opTag ) const { return op_stats[ opTag ]; }
    //cumulative stats for users per rank
    const rc_payer_stats& get_payer_stats( int payerRank ) const { return payer_stats[ payerRank ]; }
    //average cost of vote/comment/transfer at the starting block (used for payer affordability filter)
    int64_t get_archive_average_cost( int opTag ) const { return average_cost[opTag]; }

  private:
    uint32_t block_num = 0;
    int64_t regen = 0;
    resource_count_type budget;
    resource_count_type pool;
    resource_share_type share;
    fc::int_array< rc_op_stats, HIVE_RC_NUM_OPERATIONS + 1 > op_stats;
    fc::int_array< rc_payer_stats, HIVE_RC_NUM_PAYER_RANKS > payer_stats;
    fc::int_array< int64_t, 3 > average_cost;

  CHAINBASE_UNPACK_CONSTRUCTOR( rc_stats_object );
};
typedef oid_ref< rc_stats_object > rc_stats_id_type;
const rc_stats_id_type RC_PENDING_STATS_ID( oid< rc_stats_object >(0) );
const rc_stats_id_type RC_ARCHIVE_STATS_ID( oid< rc_stats_object >(1) );

/**
  * Represents temporary data on pending transactions (singleton).
  * Stored as chain object to utilize undo mechanism.
  */
class rc_pending_data : public object< rc_pending_data_type, rc_pending_data >
{
  CHAINBASE_OBJECT( rc_pending_data );
  public:
    template< typename Allocator >
    rc_pending_data( allocator< Allocator > a, uint64_t _id )
      : id( _id ) {}

    //resets usage counters for selected existing state - should be called in pre-apply transaction
    void reset_differential_usage()
    {
      differential_usage = {};
    }
    //accumulates differential RC usage ("negative" usage of selected state that will soon be modified)
    void add_differential_usage( const resource_count_type& usage )
    {
      for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
        differential_usage[i] -= usage[i];
    }
    //accumulates extra RC usage from custom ops interpreted by hived plugin(s)
    void add_custom_op_usage( const resource_count_type& usage )
    {
      for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
        differential_usage[i] += usage[i];
    }

    //resets pending usage and cost counters - should be called in pre-apply block
    void reset_pending_usage()
    {
      tx_count = 0;
      pending_usage = {};
      pending_cost = {};
    }
    //accumulates RC usage and cost of pending transaction
    void add_pending_usage( const resource_count_type& usage, const resource_cost_type& cost )
    {
      ++tx_count;
      for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
      {
        pending_usage[i] += usage[i];
        pending_cost[i] += cost[i];
      }
    }

    //(negative) usage counters for selected existing state since last reset
    const resource_count_type& get_differential_usage() const { return differential_usage; }

    //number of transactions/automated actions included in pending usage/cost
    uint32_t get_tx_count() const { return tx_count; }
    //usage counters since last reset
    const resource_count_type& get_pending_usage() const { return pending_usage; }
    //cost counters since last reset
    const resource_cost_type& get_pending_cost() const { return pending_cost; }

  private:
    //number of transactions since last reset in pre-apply block (paired with pending_usage/pending_cost)
    uint32_t tx_count = 0;
    //resources consumed by last transactions
    resource_count_type pending_usage;
    //cost of resources accumulated in pending_usage (for logging purposes)
    resource_cost_type pending_cost;

    //negative value of resources used by selected existing state before transaction is applied (since reset in pre-apply transaction)
    resource_count_type differential_usage; //see prepare_differential_usage routine for detailed description

  CHAINBASE_UNPACK_CONSTRUCTOR( rc_pending_data );
};
typedef oid_ref< rc_pending_data > rc_pending_data_id_type;

class rc_account_object : public object< rc_account_object_type, rc_account_object >
{
  CHAINBASE_OBJECT( rc_account_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_account_object )

    //RC compensation for account creation fee
    share_type get_rc_adjustment() const { return rc_adjustment; }
    //RC that were delegated to other accounts
    share_type get_delegated_rc() const { return delegated_rc; }
    //RC that were borrowed from other accounts
    share_type get_received_rc() const { return received_rc; }

    account_name_type            account;
    hive::chain::util::manabar   rc_manabar;
    share_type                   rc_adjustment;

    // This is used for bug-catching, to match that the vesting shares in a
    // pre-op are equal to what they were at the last post-op.
    share_type                   last_max_rc;

    share_type                   delegated_rc;
    share_type                   received_rc;
};
typedef oid_ref< rc_account_object > rc_account_id_type;


class rc_direct_delegation_object : public object< rc_direct_delegation_object_type, rc_direct_delegation_object >
{
  CHAINBASE_OBJECT( rc_direct_delegation_object );
  public:
    template< typename Allocator >
    rc_direct_delegation_object( allocator< Allocator > a, uint64_t _id,
      const account_object& _from, const account_object& _to, uint64_t _delegated_rc )
    : id( _id ), from( _from.get_id() ), to( _to.get_id() ), delegated_rc( _delegated_rc ) {}

    account_id_type from;
    account_id_type to;
    uint64_t        delegated_rc = 0;
  CHAINBASE_UNPACK_CONSTRUCTOR(rc_direct_delegation_object);
};
typedef oid_ref< rc_direct_delegation_object > rc_direct_delegtion_id_type;

int64_t get_maximum_rc( const hive::chain::account_object& account, const rc_account_object& rc_account, bool only_delegable_rc = false );
void update_rc_account_after_delegation( database& _db, const rc_account_object& rc_account, const account_object& account, uint32_t now, int64_t delta, bool regenerate_mana = false );

/**
  * When delegation overflow happens (see check_for_rc_delegation_overflow) some direct rc delegations need to be removed.
  * If the removal process hits limit (too many of them need to be removed) the remaining delegations are moved to new
  * object of expired delegation, that is, original delegator behaves as if delegations were properly removed (*), but some
  * remain active. They are to be removed in consecutive blocks.
  * (*) When there is still expired delegation object for selected delegator, that account cannot create new delegations.
  */
class rc_expired_delegation_object : public object< rc_expired_delegation_object_type, rc_expired_delegation_object >
{
  CHAINBASE_OBJECT( rc_expired_delegation_object );
  public:
  template< typename Allocator >
    rc_expired_delegation_object( allocator< Allocator > a, uint64_t _id,
      const account_object& _from, const uint64_t& _expired_delegation )
    : id( _id ), from( _from.get_id() ), expired_delegation( _expired_delegation ) {}

    account_id_type from;
    uint64_t        expired_delegation = 0;
  CHAINBASE_UNPACK_CONSTRUCTOR( rc_expired_delegation_object );
};
typedef oid_ref< rc_expired_delegation_object > rc_expired_delegtion_id_type;

bool has_expired_delegation( const database& _db, const account_object& account );
void remove_delegations( database& _db, int64_t& delegation_overflow, account_id_type delegator_id, uint32_t now, remove_guard& obj_perf );

/**
  * Holds HIVE_RC_BUCKET_TIME_LENGTH of cumulative usage of resources.
  * There is always HIVE_RC_WINDOW_BUCKET_COUNT of buckets.
  * The data is used (indirectly) to split global regeneration (new rc currency in circulation) between
  * various resource pools based on past demand within time window (making "more popular" resources
  * relatively more expensive). Note that sum of usage from all buckets is stored in rc_pool_object.
  */
class rc_usage_bucket_object : public object< rc_usage_bucket_object_type, rc_usage_bucket_object >
{
  CHAINBASE_OBJECT( rc_usage_bucket_object );
  public:
    template< typename Allocator >
    rc_usage_bucket_object( allocator< Allocator > a, uint64_t _id,
      const time_point_sec& _timestamp )
      : id( _id ), timestamp( _timestamp )
    {}

    //resets bucket to represent new window fragment
    void reset( const time_point_sec& _timestamp )
    {
      timestamp = _timestamp;
      usage = {};
    }
    //adds usage data on given resource
    void add_usage( int poolIdx, int64_t resource_consumed )
    {
      usage[ poolIdx ] += resource_consumed;
    }

    //timestamp of first block that this bucket refers to
    time_point_sec get_timestamp() const { return timestamp; }
    //accumulated amount of resources consumed in the fragment of the window represented by this bucket
    const resource_count_type& get_usage() const { return usage; }
    //same as above but for selected resource
    int64_t get_usage( int i ) const { return usage[i]; }

  private:
    time_point_sec timestamp;
    resource_count_type usage;

  CHAINBASE_UNPACK_CONSTRUCTOR( rc_usage_bucket_object );
};
typedef oid_ref< rc_usage_bucket_object > rc_cost_bucket_id_type;

typedef multi_index_container<
  rc_resource_param_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_resource_param_object, rc_resource_param_object::id_type, &rc_resource_param_object::get_id > >
  >,
  allocator< rc_resource_param_object >
> rc_resource_param_index;

typedef multi_index_container<
  rc_pool_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_pool_object, rc_pool_object::id_type, &rc_pool_object::get_id > >
  >,
  allocator< rc_pool_object >
> rc_pool_index;

typedef multi_index_container<
  rc_stats_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_stats_object, rc_stats_object::id_type, &rc_stats_object::get_id > >
  >,
  allocator< rc_stats_object >
> rc_stats_index;

typedef multi_index_container<
  rc_pending_data,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_pending_data, rc_pending_data::id_type, &rc_pending_data::get_id > >
  >,
  allocator< rc_pending_data >
> rc_pending_data_index;

typedef multi_index_container<
  rc_account_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_account_object, rc_account_object::id_type, &rc_account_object::get_id > >,
    ordered_unique< tag< by_name >,
      member< rc_account_object, account_name_type, &rc_account_object::account > >
  >,
  allocator< rc_account_object >
> rc_account_index;

struct by_from_to;

typedef multi_index_container<
  rc_direct_delegation_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_direct_delegation_object, rc_direct_delegation_object::id_type, &rc_direct_delegation_object::get_id > >,
    ordered_unique< tag< by_from_to >,
      composite_key< rc_direct_delegation_object,
        member< rc_direct_delegation_object, account_id_type, &rc_direct_delegation_object::from >,
        member< rc_direct_delegation_object, account_id_type, &rc_direct_delegation_object::to >
      >
    >
  >,
  allocator< rc_direct_delegation_object >
> rc_direct_delegation_index;

typedef multi_index_container<
  rc_expired_delegation_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_expired_delegation_object, rc_expired_delegation_object::id_type, &rc_expired_delegation_object::get_id > >,
    ordered_unique< tag< by_account >,
      member< rc_expired_delegation_object, account_id_type, &rc_expired_delegation_object::from > >
  >,
  allocator< rc_expired_delegation_object >
> rc_expired_delegation_index;

struct by_timestamp;

typedef multi_index_container<
  rc_usage_bucket_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_usage_bucket_object, rc_usage_bucket_object::id_type, &rc_usage_bucket_object::get_id > >,
    ordered_unique< tag< by_timestamp >,
      const_mem_fun< rc_usage_bucket_object, time_point_sec, &rc_usage_bucket_object::get_timestamp > >
  >,
  allocator< rc_usage_bucket_object >
> rc_usage_bucket_index;

} } } // hive::plugins::rc

FC_REFLECT( hive::plugins::rc::rc_resource_param_object, (id)(resource_param_array) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_resource_param_object, hive::plugins::rc::rc_resource_param_index )

FC_REFLECT( hive::plugins::rc::rc_pool_object,
  (id)
  (pool_array)
  (usage_in_window)
  (last_known_budget)
  (resource_weights)
  (sum_of_resource_weights)
)
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_pool_object, hive::plugins::rc::rc_pool_index )

FC_REFLECT( hive::plugins::rc::rc_stats_object,
  (id)
  (block_num)
  (regen)
  (budget)
  (pool)
  (share)
  (op_stats)
  (payer_stats)
  (average_cost)
)
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_stats_object, hive::plugins::rc::rc_stats_index )

FC_REFLECT( hive::plugins::rc::rc_pending_data,
  (id)
  (tx_count)
  (pending_usage)
  (pending_cost)
  (differential_usage)
)
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_pending_data, hive::plugins::rc::rc_pending_data_index )

FC_REFLECT( hive::plugins::rc::rc_account_object,
  (id)
  (account)
  (rc_manabar)
  (rc_adjustment)
  (last_max_rc)
  (delegated_rc)
  (received_rc)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_account_object, hive::plugins::rc::rc_account_index )

FC_REFLECT( hive::plugins::rc::rc_direct_delegation_object,
  (id)
  (from)
  (to)
  (delegated_rc)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_direct_delegation_object, hive::plugins::rc::rc_direct_delegation_index )

FC_REFLECT( hive::plugins::rc::rc_expired_delegation_object,
  (id)
  (from)
  (expired_delegation)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_expired_delegation_object, hive::plugins::rc::rc_expired_delegation_index )

FC_REFLECT( hive::plugins::rc::rc_usage_bucket_object,
  (id)
  (timestamp)
  (usage)
)
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_usage_bucket_object, hive::plugins::rc::rc_usage_bucket_index )

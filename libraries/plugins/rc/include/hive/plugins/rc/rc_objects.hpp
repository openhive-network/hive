#pragma once
#include <hive/chain/util/manabar.hpp>

#include <hive/plugins/rc/rc_config.hpp>
#include <hive/plugins/rc/rc_utility.hpp>
#include <hive/plugins/rc/resource_count.hpp>
#include <hive/chain/database.hpp>

#include <hive/chain/hive_object_types.hpp>

#include <fc/int_array.hpp>

namespace hive { namespace chain {
struct by_account;
} }

namespace hive { namespace plugins { namespace rc {

using namespace std;
using namespace hive::chain;
using hive::protocol::asset;

enum rc_object_types
{
  rc_resource_param_object_type    = ( HIVE_RC_SPACE_ID << 8 ),
  rc_pool_object_type              = ( HIVE_RC_SPACE_ID << 8 ) + 1,
  rc_account_object_type           = ( HIVE_RC_SPACE_ID << 8 ) + 2,
  rc_direct_delegation_object_type = ( HIVE_RC_SPACE_ID << 8 ) + 3,
  rc_usage_bucket_object_type      = ( HIVE_RC_SPACE_ID << 8 ) + 4,
  rc_pending_data_type             = ( HIVE_RC_SPACE_ID << 8 ) + 5
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

  private:
    resource_count_type pool_array;
    resource_count_type usage_in_window;
    fc::int_array< uint64_t, HIVE_RC_NUM_RESOURCE_TYPES > resource_weights; //in basis points of respective block-budgets
    uint64_t sum_of_resource_weights = 1; //should never be zero

  CHAINBASE_UNPACK_CONSTRUCTOR( rc_pool_object );
};
typedef oid_ref< rc_pool_object > rc_pool_id_type;

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
      : id( _id ), tx_count( 0 ) {}

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

    //number of transactions/automated actions included in pending usage/cost
    uint32_t get_tx_count() const { return tx_count; }
    //usage counters since last reset
    const resource_count_type& get_pending_usage() const { return pending_usage; }
    //cost counters since last reset
    const resource_cost_type& get_pending_cost() const { return pending_cost; }

  private:
    uint32_t tx_count;
    resource_count_type pending_usage; //resources consumed by last transactions (since reset in pre-apply block)
    resource_cost_type pending_cost; //cost of resources accumulated in pending_usage (for logging purposes)

  CHAINBASE_UNPACK_CONSTRUCTOR( rc_pending_data );
};
typedef oid_ref< rc_pending_data > rc_pending_data_id_type;

class rc_account_object : public object< rc_account_object_type, rc_account_object >
{
  CHAINBASE_OBJECT( rc_account_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_account_object )

    account_name_type            account;
    hive::chain::util::manabar   rc_manabar;
    asset                        max_rc_creation_adjustment = asset( 0, VESTS_SYMBOL );

    // This is used for bug-catching, to match that the vesting shares in a
    // pre-op are equal to what they were at the last post-op.
    int64_t                      last_max_rc = 0;

    uint64_t                     delegated_rc = 0;
    uint64_t                     received_delegated_rc = 0;
};
typedef oid_ref< rc_account_object > rc_account_id_type;


class rc_direct_delegation_object : public object< rc_direct_delegation_object_type, rc_direct_delegation_object >
{
  CHAINBASE_OBJECT( rc_direct_delegation_object );
  public:
    template< typename Allocator >
    rc_direct_delegation_object( allocator< Allocator > a, uint64_t _id,
      const account_id_type& _from, const account_id_type& _to, const uint64_t& _delegated_rc)
    : id( _id ), from(_from), to(_to), delegated_rc(_delegated_rc) {}

    account_id_type from;
    account_id_type to;
    uint64_t        delegated_rc = 0;
  CHAINBASE_UNPACK_CONSTRUCTOR(rc_direct_delegation_object);
};

int64_t get_maximum_rc( const hive::chain::account_object& account, const rc_account_object& rc_account, bool only_delegable_rc = false );
void update_rc_account_after_delegation( database& _db, const rc_account_object& rc_account, const account_object& account, uint32_t now, int64_t delta, bool regenerate_mana = false );

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
> rc_direct_delegation_object_index;

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
  (resource_weights)
  (sum_of_resource_weights)
)
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_pool_object, hive::plugins::rc::rc_pool_index )

FC_REFLECT( hive::plugins::rc::rc_pending_data,
  (id)
  (tx_count)
  (pending_usage)
  (pending_cost)
)
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_pending_data, hive::plugins::rc::rc_pending_data_index )

FC_REFLECT( hive::plugins::rc::rc_account_object,
  (id)
  (account)
  (rc_manabar)
  (max_rc_creation_adjustment)
  (last_max_rc)
  (delegated_rc)
  (received_delegated_rc)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_account_object, hive::plugins::rc::rc_account_index )

FC_REFLECT( hive::plugins::rc::rc_direct_delegation_object,
  (id)
  (from)
  (to)
  (delegated_rc)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_direct_delegation_object, hive::plugins::rc::rc_direct_delegation_object_index )

FC_REFLECT( hive::plugins::rc::rc_usage_bucket_object,
  (id)
  (timestamp)
  (usage)
)
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_usage_bucket_object, hive::plugins::rc::rc_usage_bucket_index )

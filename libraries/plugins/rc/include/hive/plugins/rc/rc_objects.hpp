#pragma once
#include <hive/chain/util/manabar.hpp>

#include <hive/plugins/rc/rc_config.hpp>
#include <hive/plugins/rc/rc_utility.hpp>
#include <hive/plugins/rc/resource_count.hpp>

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
  rc_resource_param_object_type   = ( HIVE_RC_SPACE_ID << 8 ),
  rc_pool_object_type             = ( HIVE_RC_SPACE_ID << 8 ) + 1,
  rc_account_object_type          = ( HIVE_RC_SPACE_ID << 8 ) + 2,
  rc_delegation_pool_object_type  = ( HIVE_RC_SPACE_ID << 8 ) + 3,
  rc_indel_edge_object_type       = ( HIVE_RC_SPACE_ID << 8 ) + 4,
  rc_outdel_drc_edge_object_type  = ( HIVE_RC_SPACE_ID << 8 ) + 5,
  rc_usage_bucket_object_type     = ( HIVE_RC_SPACE_ID << 8 ) + 6
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

    //current content of resource pools
    resource_count_type pool_array;
  private:
    resource_count_type usage_in_window;
    fc::int_array< uint64_t, HIVE_RC_NUM_RESOURCE_TYPES > resource_weights; //in basis points of respective block-budgets
    uint64_t sum_of_resource_weights = 1; //should never be zero

  CHAINBASE_UNPACK_CONSTRUCTOR( rc_pool_object );
};
typedef oid_ref< rc_pool_object > rc_pool_id_type;

class rc_account_object : public object< rc_account_object_type, rc_account_object >
{
  CHAINBASE_OBJECT( rc_account_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_account_object )

    account_name_type     account;
    hive::chain::util::manabar   rc_manabar;
    asset                 max_rc_creation_adjustment = asset( 0, VESTS_SYMBOL );

    // This is used for bug-catching, to match that the vesting shares in a
    // pre-op are equal to what they were at the last post-op.
    int64_t               last_max_rc = 0;
};
typedef oid_ref< rc_account_object > rc_account_id_type;

/**
  * Represents a delegation pool.
  */
class rc_delegation_pool_object : public object< rc_delegation_pool_object_type, rc_delegation_pool_object >
{
  CHAINBASE_OBJECT( rc_delegation_pool_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_delegation_pool_object )

    account_name_type             account;
    hive::chain::util::manabar   rc_pool_manabar;
};

/**
  * Represents a delegation from a user to a pool.
  */
class rc_indel_edge_object : public object< rc_indel_edge_object_type, rc_indel_edge_object >
{
  CHAINBASE_OBJECT( rc_indel_edge_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_indel_edge_object )

    account_name_type             from_account;
    account_name_type             to_pool;
    share_type                    amount;
};

/**
  * Represents a delegation from a pool to a user based on delegated resource credits (DRC).
  *
  * In the case of a pool that is not under heavy load, DRC:RC has a 1:1 exchange rate.
  *
  * However, if the pool drops below HIVE_RC_DRC_FLOAT_LEVEL, DRC:RC exchange rate starts
  * to rise according to `f(x) = 1/(a+b*x)` where `x` is the pool level, and coefficients `a`,
  * `b` are set such that `f(HIVE_RC_DRC_FLOAT_LEVEL) = 1` and `f(0) = HIVE_RC_MAX_DRC_RATE`.
  *
  * This ensures the limited RC of oversubscribed pools under heavy load are
  * shared "fairly" among their users proportionally to DRC.  This logic
  * provides a smooth transition between the "fiat regime" (i.e. DRC represent
  * a direct allocation of RC) and the "proportional regime" (i.e. DRC represent
  * the fraction of RC that the user is allowed).
  */
class rc_outdel_drc_edge_object : public object< rc_outdel_drc_edge_object_type, rc_outdel_drc_edge_object >
{
  CHAINBASE_OBJECT( rc_outdel_drc_edge_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_outdel_drc_edge_object )

    account_name_type             from_pool;
    account_name_type             to_account;
    hive::chain::util::manabar    drc_manabar;
    int64_t                       drc_max_mana = 0;
};

int64_t get_maximum_rc( const hive::chain::account_object& account, const rc_account_object& rc_account );

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

struct by_edge;
struct by_timestamp;

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
  rc_account_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_account_object, rc_account_object::id_type, &rc_account_object::get_id > >,
    ordered_unique< tag< by_name >,
      member< rc_account_object, account_name_type, &rc_account_object::account > >
  >,
  allocator< rc_account_object >
> rc_account_index;

typedef multi_index_container<
  rc_delegation_pool_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_delegation_pool_object, rc_delegation_pool_object::id_type, &rc_delegation_pool_object::get_id > >,
    ordered_unique< tag< by_account >, member< rc_delegation_pool_object, account_name_type, &rc_delegation_pool_object::account > >
  >,
  allocator< rc_delegation_pool_object >
> rc_delegation_pool_index;

typedef multi_index_container<
  rc_indel_edge_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_indel_edge_object, rc_indel_edge_object::id_type, &rc_indel_edge_object::get_id > >,
    ordered_unique< tag< by_edge >,
        composite_key< rc_indel_edge_object,
          member< rc_indel_edge_object, account_name_type, &rc_indel_edge_object::from_account >,
          member< rc_indel_edge_object, account_name_type, &rc_indel_edge_object::to_pool >
        >
    >
  >,
  allocator< rc_indel_edge_object >
> rc_indel_edge_index;

typedef multi_index_container<
  rc_outdel_drc_edge_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_outdel_drc_edge_object, rc_outdel_drc_edge_object::id_type, &rc_outdel_drc_edge_object::get_id > >,
    ordered_unique< tag< by_edge >,
        composite_key< rc_outdel_drc_edge_object,
          member< rc_outdel_drc_edge_object, account_name_type, &rc_outdel_drc_edge_object::from_pool >,
          member< rc_outdel_drc_edge_object, account_name_type, &rc_outdel_drc_edge_object::to_account >
        >
    >
  >,
  allocator< rc_outdel_drc_edge_object >
> rc_outdel_drc_edge_index;

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

FC_REFLECT( hive::plugins::rc::rc_account_object,
  (id)
  (account)
  (rc_manabar)
  (max_rc_creation_adjustment)
  (last_max_rc)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_account_object, hive::plugins::rc::rc_account_index )

FC_REFLECT( hive::plugins::rc::rc_delegation_pool_object,
  (id)
  (account)
  (rc_pool_manabar)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_delegation_pool_object, hive::plugins::rc::rc_delegation_pool_index )

FC_REFLECT( hive::plugins::rc::rc_indel_edge_object,
  (id)
  (from_account)
  (to_pool)
  (amount)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_indel_edge_object, hive::plugins::rc::rc_indel_edge_index )

FC_REFLECT( hive::plugins::rc::rc_outdel_drc_edge_object,
  (id)
  (from_pool)
  (to_account)
  (drc_manabar)
  (drc_max_mana)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_outdel_drc_edge_object, hive::plugins::rc::rc_outdel_drc_edge_index )

FC_REFLECT( hive::plugins::rc::rc_usage_bucket_object,
  (id)
  (timestamp)
  (usage)
)
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_usage_bucket_object, hive::plugins::rc::rc_usage_bucket_index )

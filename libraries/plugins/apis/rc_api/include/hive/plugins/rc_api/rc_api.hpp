#pragma once
#include <hive/plugins/json_rpc/utility.hpp>
#include <hive/plugins/rc/rc_objects.hpp>

#include <hive/protocol/types.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>

#define RC_API_DEFAULT_QUERY_LIMIT 0
#define RC_API_SINGLE_QUERY_LIMIT 1000

namespace hive { namespace plugins { namespace rc {

namespace detail
{
  class rc_api_impl;
}

struct list_object_args_type
{
  fc::variant       start;
  uint32_t          limit = RC_API_DEFAULT_QUERY_LIMIT;
};

using plugins::json_rpc::void_type;

typedef void_type get_resource_params_args;

struct get_resource_params_return
{
  vector< string >                                         resource_names;
  variant_object                                           resource_params;
  variant_object                                           size_info;
};

typedef void_type get_resource_pool_args;

struct resource_pool_api_object
{
  int64_t pool = 0;
  uint16_t fill_level = 0; //how much (in bps) does current pool stand vs pool equilibrium value
};

struct get_resource_pool_return
{
  variant_object                                           resource_pool;
};

struct rc_account_api_object
{
  rc_account_api_object(){}

  rc_account_api_object( const rc_account_object& rca, const database& db ) :
          account( rca.account ),
          rc_manabar( rca.rc_manabar ),
          max_rc_creation_adjustment( rca.get_rc_adjustment(), VESTS_SYMBOL ),
          delegated_rc( rca.get_delegated_rc().value ),
          received_delegated_rc( rca.get_received_rc().value )
  {
    max_rc = get_maximum_rc( db.get_account( account ), rca );
  }

  account_name_type     account;
  hive::chain::util::manabar   rc_manabar;
  asset                 max_rc_creation_adjustment = asset( 0, VESTS_SYMBOL );
  int64_t               max_rc = 0;
  uint64_t              delegated_rc = 0;
  uint64_t              received_delegated_rc = 0;

  //
  // This is used for bug-catching, to match that the vesting shares in a
  // pre-op are equal to what they were at the last post-op.
  //
  // Don't return it in the API for now, because we don't want
  // API's to be built to assume its presence
  //
  // If it's needed it for debugging, we might think about un-commenting it
  //
  // asset                 last_vesting_shares = asset( 0, VESTS_SYMBOL );
};

struct rc_direct_delegation_api_object
{
  rc_direct_delegation_api_object(){}

  rc_direct_delegation_api_object( const rc_direct_delegation_object& rcdd, const account_name_type& _from, account_name_type _to ) :
          from_id( rcdd.from ),
          to_id( rcdd.to ),
          from(_from),
          to(_to),
          delegated_rc( rcdd.delegated_rc ) {}

  account_id_type from_id;
  account_id_type to_id;
  account_name_type from;
  account_name_type to;
  uint64_t        delegated_rc = 0;
};

struct find_rc_accounts_args
{
  std::vector< account_name_type >                         accounts;
};

struct find_rc_accounts_return
{
  std::vector< rc_account_api_object >                     rc_accounts;
};

typedef list_object_args_type list_rc_accounts_args;

typedef find_rc_accounts_return list_rc_accounts_return;

typedef list_object_args_type list_rc_direct_delegations_args;

struct list_rc_direct_delegations_return
{
  std::vector< rc_direct_delegation_api_object > rc_direct_delegations;
};

typedef void_type get_rc_stats_args;

struct get_rc_stats_return
{
  variant_object rc_stats;
};

struct get_rc_operation_stats_args
{
  std::string operation; //behaves like enum, except hive::protocol::operation is not an enum
};

struct get_rc_operation_stats_return_cost
{
  int64_t history_rc;
  int64_t tokens_rc;
  int64_t market_rc;
  int64_t state_rc;
  int64_t exec_rc;
};
struct get_rc_operation_stats_return_share
{
  uint16_t history_bp;
  uint16_t tokens_bp;
  uint16_t market_bp;
  uint16_t state_bp;
  uint16_t exec_bp;
};
struct get_rc_operation_stats_return_usage
{
  int64_t history_bytes;
  std::string tokens; //not a double to force format with fixed precision
  int64_t market_bytes;
  int64_t state_hbytes;
  int64_t exec_ns;
};
struct get_rc_operation_stats_return
{
  uint32_t count;
  int64_t avg_cost_rc;
  get_rc_operation_stats_return_cost resource_cost;
  get_rc_operation_stats_return_share resource_cost_share;
  get_rc_operation_stats_return_usage resource_usage;
};

class rc_api
{
  public:
    rc_api();
    ~rc_api();

    DECLARE_API(
      (get_resource_params)
      (get_resource_pool)
      (find_rc_accounts)
      (list_rc_accounts)
      (list_rc_direct_delegations)
      (get_rc_stats)
      (get_rc_operation_stats)
      )

  private:
    std::unique_ptr< detail::rc_api_impl > my;
};

} } } // hive::plugins::rc

FC_REFLECT( hive::plugins::rc::list_object_args_type,
            (start)
            (limit)
            )

FC_REFLECT( hive::plugins::rc::get_resource_params_return,
  (resource_names)
  (resource_params)
  (size_info)
  )

FC_REFLECT( hive::plugins::rc::resource_pool_api_object,
  (pool)
  (fill_level)
  )

FC_REFLECT( hive::plugins::rc::get_resource_pool_return,
  (resource_pool)
  )

FC_REFLECT( hive::plugins::rc::rc_account_api_object,
  (account)
  (rc_manabar)
  (max_rc_creation_adjustment)
  (max_rc)
  (delegated_rc)
  (received_delegated_rc)
  )

FC_REFLECT( hive::plugins::rc::rc_direct_delegation_api_object,
  (from)
  (to)
  (delegated_rc)
  )

FC_REFLECT( hive::plugins::rc::find_rc_accounts_args,
  (accounts)
  )

FC_REFLECT( hive::plugins::rc::find_rc_accounts_return,
  (rc_accounts)
  )

FC_REFLECT( hive::plugins::rc::list_rc_direct_delegations_return,
  (rc_direct_delegations)
  )

FC_REFLECT( hive::plugins::rc::get_rc_stats_return,
  (rc_stats)
  )

FC_REFLECT( hive::plugins::rc::get_rc_operation_stats_args,
  (operation)
  )

FC_REFLECT( hive::plugins::rc::get_rc_operation_stats_return_cost,
  (history_rc)
  (tokens_rc)
  (market_rc)
  (state_rc)
  (exec_rc)
  )
FC_REFLECT( hive::plugins::rc::get_rc_operation_stats_return_share,
  (history_bp)
  (tokens_bp)
  (market_bp)
  (state_bp)
  (exec_bp)
  )
FC_REFLECT( hive::plugins::rc::get_rc_operation_stats_return_usage,
  (history_bytes)
  (tokens)
  (market_bytes)
  (state_hbytes)
  (exec_ns)
  )
FC_REFLECT( hive::plugins::rc::get_rc_operation_stats_return,
  (count)
  (avg_cost_rc)
  (resource_cost)
  (resource_cost_share)
  (resource_usage)
  )

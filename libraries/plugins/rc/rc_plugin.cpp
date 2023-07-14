
#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/config.hpp>
#include <hive/chain/rc/rc_curve.hpp>
#include <hive/chain/rc/rc_export_objects.hpp>
#include <hive/plugins/rc/rc_plugin.hpp>
#include <hive/chain/rc/rc_objects.hpp>
#include <hive/chain/rc/rc_operations.hpp>

#include <hive/chain/rc/resource_count.hpp>
#include <hive/chain/rc/resource_user.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/generic_custom_operation_interpreter.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/chain/util/remove_guard.hpp>

#include <hive/jsonball/jsonball.hpp>

#include <boost/algorithm/string.hpp>
#include <cmath>

#ifndef IS_TEST_NET
#define HIVE_HF20_BLOCK_NUM 26256743
#endif

namespace hive { namespace plugins { namespace rc {

using namespace hive::chain;

namespace detail {

using chain::plugin_exception;
using hive::chain::util::manabar_params;

class rc_plugin_impl
{
  public:
    rc_plugin_impl( rc_plugin& _plugin ) :
      _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ),
      _self( _plugin )
    {}

    void on_pre_apply_operation( const operation_notification& note );
    void on_post_apply_operation( const operation_notification& note );

    database&                     _db;
    rc_plugin&                    _self;

    boost::signals2::connection   _pre_apply_operation_conn;
    boost::signals2::connection   _post_apply_operation_conn;
};

//
// This visitor performs the following functions:
// - Call regenerate() when an account's vesting shares are about to change
//
struct pre_apply_operation_visitor
{
  typedef void result_type;

  database&                                _db;
  uint32_t                                 _current_time = 0;

  pre_apply_operation_visitor( database& db ) : _db(db)
  {
    const auto& gpo = _db.get_dynamic_global_properties();
    _current_time = gpo.time.sec_since_epoch();
  }

  template< bool account_may_not_exist = false >
  void regenerate( const account_name_type& name )const
  {
    const account_object* account = _db.find_account( name );
    if( account_may_not_exist )
    {
      if( account == nullptr )
        return;
    }
    else
    {
      FC_ASSERT( account != nullptr, "Unexpectedly, account ${a} does not exist", ("a", name) );
    }

    _db.rc.regenerate_rc_mana( *account, _current_time );
  }

  void operator()( const account_create_with_delegation_operation& op )const
  {
    regenerate( op.creator );
  }

  void operator()( const transfer_to_vesting_operation& op )const
  {
    account_name_type target = op.to.size() ? op.to : op.from;
    regenerate( target );
  }

  void operator()( const withdraw_vesting_operation& op )const
  {
    regenerate( op.account );
  }

  void operator()( const set_withdraw_vesting_route_operation& op )const
  {
    regenerate( op.from_account );
  }

  void operator()( const delegate_vesting_shares_operation& op )const
  {
    regenerate( op.delegator );
    regenerate( op.delegatee );
  }

  void operator()( const author_reward_operation& op )const
  {
    regenerate( op.author );
  }

  void operator()( const curation_reward_operation& op )const
  {
    regenerate( op.curator );
  }

  // Is this one actually necessary?
  void operator()( const comment_reward_operation& op )const
  {
    regenerate( op.author );
  }

  void operator()( const fill_vesting_withdraw_operation& op )const
  {
    regenerate( op.from_account );
    regenerate( op.to_account );
  }

  void operator()( const claim_reward_balance_operation& op )const
  {
    regenerate( op.account );
  }

#ifdef HIVE_ENABLE_SMT
  void operator()( const claim_reward_balance2_operation& op )const
  {
    regenerate( op.account );
  }
#endif

  void operator()( const hardfork_operation& op )const
  {
    if( op.hardfork_id == HIVE_HARDFORK_0_1 )
    {
      const auto& idx = _db.get_index< account_index >().indices().get< by_id >();
      for( auto it=idx.begin(); it!=idx.end(); ++it )
      {
        regenerate( it->get_name() );
      }
    }
  }

  void operator()( const hardfork_hive_operation& op )const
  {
    regenerate( op.account );
    for( auto& account : op.other_affected_accounts )
      regenerate( account );
  }

  void operator()( const return_vesting_delegation_operation& op )const
  {
    regenerate( op.account );
  }

  void operator()( const comment_benefactor_reward_operation& op )const
  {
    regenerate( op.benefactor );
  }

  void operator()( const producer_reward_operation& op )const
  {
    regenerate( op.producer );
  }

  void operator()( const clear_null_account_balance_operation& op )const
  {
    regenerate( HIVE_NULL_ACCOUNT );
  }

  //void operator()( const consolidate_treasury_balance_operation& op )const //not needed for treasury accounts, leave default

  //void operator()( const delayed_voting_operation& op )const //not needed, only voting power changes, not amount of VESTs

  //void operator()( const pow_operation& op )const //not needed, RC not active when pow was active

  //void operator()( const pow2_operation& op )const //not needed, RC not active when pow2 was active

  template< typename Op >
  void operator()( const Op& op )const {}
};

struct post_apply_operation_visitor
{
  typedef void result_type;

  database&                                _db;
  uint32_t                                 _current_time = 0;

  post_apply_operation_visitor( database& db ) : _db(db)
  {
    const auto& dgpo = _db.get_dynamic_global_properties();
    _current_time = dgpo.time.sec_since_epoch();
  }

  void update_after_vest_change( const account_name_type& account_name,
    bool _fill_new_mana = true, bool _check_for_rc_delegation_overflow = false ) const
  {
    const account_object& account = _db.get_account( account_name );
    _db.rc.update_account_after_vest_change( account, _current_time, _fill_new_mana,
      _check_for_rc_delegation_overflow );
  }
  void operator()( const account_create_with_delegation_operation& op )const
  {
    update_after_vest_change( op.creator );
  }

  //void operator()( const pow_operation& op )const //not needed, RC not active when pow was active

  //void operator()( const pow2_operation& op )const //not needed, RC not active when pow2 was active

  void operator()( const transfer_to_vesting_operation& op )
  {
    account_name_type target = op.to.size() ? op.to : op.from;
    update_after_vest_change( target );
  }

  void operator()( const withdraw_vesting_operation& op )const
  {
    update_after_vest_change( op.account, false, true );
  }

  void operator()( const delegate_vesting_shares_operation& op )const
  {
    update_after_vest_change( op.delegator, true, true );
    update_after_vest_change( op.delegatee, true, true );
  }

  void operator()( const author_reward_operation& op )const
  {
    update_after_vest_change( op.author );
      //not needed, since HF17 rewards need to be claimed before they affect vest balance
  }

  void operator()( const curation_reward_operation& op )const
  {
    update_after_vest_change( op.curator );
      //not needed, since HF17 rewards need to be claimed before they affect vest balance
  }

  void operator()( const fill_vesting_withdraw_operation& op )const
  {
    update_after_vest_change( op.from_account, true, true );
    update_after_vest_change( op.to_account );
  }

  void operator()( const claim_reward_balance_operation& op )const
  {
    update_after_vest_change( op.account );
  }

#ifdef HIVE_ENABLE_SMT
  void operator()( const claim_reward_balance2_operation& op )const
  {
    update_after_vest_change( op.account );
  }
#endif

  void operator()( const hardfork_operation& op )const
  {
    if( op.hardfork_id == HIVE_HARDFORK_0_1 )
    {
      const auto& idx = _db.get_index< account_index, by_id >();
      for( auto it=idx.begin(); it!=idx.end(); ++it )
        update_after_vest_change( it->get_name() );
    }

    if( op.hardfork_id == HIVE_HARDFORK_0_20 )
    {
      const auto& params = _db.get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );

      _db.modify( _db.get< rc_pool_object, by_id >( rc_pool_id_type() ), [&]( rc_pool_object& p )
      {
        for( size_t i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; i++ )
        {
          p.set_pool( i, int64_t( params.resource_param_array[ i ].resource_dynamics_params.max_pool_size ) );
        }

        p.set_pool( resource_new_accounts, 0 );
      });
    }
  }

  void operator()( const hardfork_hive_operation& op )const
  {
    update_after_vest_change( op.account, true, true );
    for( auto& account : op.other_affected_accounts )
      update_after_vest_change( account, true, true );
  }

  void operator()( const return_vesting_delegation_operation& op )const
  {
    update_after_vest_change( op.account );
  }

  void operator()( const comment_benefactor_reward_operation& op )const
  {
    update_after_vest_change( op.benefactor );
      //not needed, since HF17 rewards need to be claimed before they affect vest balance
  }

  void operator()( const producer_reward_operation& op )const
  {
    update_after_vest_change( op.producer );
  }

  void operator()( const clear_null_account_balance_operation& op )const
  {
    update_after_vest_change( HIVE_NULL_ACCOUNT );
  }

  //not needed for treasury accounts (?)
  //void operator()( const consolidate_treasury_balance_operation& op )const

  //changes power of governance vote, not vest balance
  //void operator()( const delayed_voting_operation& op )const

  //pays fee in hbd, vest balance not touched
  //void operator()( const create_proposal_operation& op )const

  //no change in vest balance
  //void operator()( const update_proposal_votes_operation& op )const

  //no change in vest balance
  //void operator()( const remove_proposal_operation& op )const

  template< typename Op >
  void operator()( const Op& op )const
  {
    // ilog( "handling post-apply operation default" );
  }
};

void rc_plugin_impl::on_pre_apply_operation( const operation_notification& note )
{ try {
  if( !_db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    return;

  pre_apply_operation_visitor vtor( _db );

  note.op.visit( vtor );

  count_resources_result differential_usage;
  if( _db.rc.prepare_differential_usage( note.op, differential_usage ) )
  {
    _db.modify( _db.get< rc_pending_data, by_id >( rc_pending_data_id_type() ), [&]( rc_pending_data& data )
    {
      data.add_differential_usage( differential_usage );
    } );
  }
  } FC_CAPTURE_AND_RETHROW( (note.op) )
}

void rc_plugin_impl::on_post_apply_operation( const operation_notification& note )
{ try {
  if( !_db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    return;
  // dlog( "Calling post-vtor on ${op}", ("op", note.op) );
  post_apply_operation_visitor vtor( _db );
  note.op.visit( vtor );
} FC_CAPTURE_AND_RETHROW( (note.op) ) }

} // detail

rc_plugin::rc_plugin() {}
rc_plugin::~rc_plugin() {}

void rc_plugin::set_program_options( options_description& cli, options_description& cfg )
{
}

void rc_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  ilog( "Initializing resource credit plugin" );

  my = std::make_unique< detail::rc_plugin_impl >( *this );

  try
  {
    chain::database& db = appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db();

    my->_pre_apply_operation_conn = db.add_pre_apply_operation_handler( [&]( const operation_notification& note )
      { try { my->on_pre_apply_operation( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_post_apply_operation_conn = db.add_post_apply_operation_handler( [&]( const operation_notification& note )
      { try { my->on_post_apply_operation( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
  }
  FC_CAPTURE_AND_RETHROW()
}

void rc_plugin::plugin_startup() {}

void rc_plugin::plugin_shutdown()
{
  chain::util::disconnect_signal( my->_pre_apply_operation_conn );
  chain::util::disconnect_signal( my->_post_apply_operation_conn );
}

} } } // hive::plugins::rc

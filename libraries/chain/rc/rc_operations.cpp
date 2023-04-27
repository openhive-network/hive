#include <hive/protocol/config.hpp>

#include <hive/chain/rc/rc_objects.hpp>
#include <hive/chain/rc/rc_operations.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/global_property_object.hpp>

#include <hive/chain/util/manabar.hpp>

#include <hive/protocol/operation_util_impl.hpp>

namespace hive { namespace chain {

void delegate_rc_operation::validate()const
{
  validate_account_name( from );
  FC_ASSERT(delegatees.size() != 0, "Must provide at least one account");
  FC_ASSERT(delegatees.size() <= HIVE_RC_MAX_ACCOUNTS_PER_DELEGATION_OP, "Provided ${size} accounts, cannot delegate to more than ${max} accounts in one operation", ("size", delegatees.size())("max", HIVE_RC_MAX_ACCOUNTS_PER_DELEGATION_OP));

  for(const account_name_type& delegatee:delegatees) {
    validate_account_name(delegatee);
    FC_ASSERT( delegatee != from, "cannot delegate rc to yourself" );
  }

  FC_ASSERT( max_rc >= 0, "amount of rc delegated cannot be negative" );
}

void delegate_rc_evaluator::do_apply( const delegate_rc_operation& op )
{
  if( !_db.has_hardfork( HIVE_HARDFORK_1_26 ) )
  {
    dlog( "Ineffective RC delegation @${b} ${op}", ( "b", _db.head_block_num() + 1 )( op ) );
    return;
  }

  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();
  if( op.max_rc != 0 && _db.is_in_control() )
  {
    const auto& wso = _db.get_witness_schedule_object();
    auto min_delegation = ( asset( wso.median_props.account_creation_fee.amount / 3, HIVE_SYMBOL ) * gpo.get_vesting_share_price() ).amount.value;

    FC_ASSERT( op.max_rc >= min_delegation, "Cannot delegate less than ${min_delegation} rc", (min_delegation) );
  }

  uint32_t now = gpo.time.sec_since_epoch();
  const account_object& from_account = _db.get_account( op.from );
  FC_ASSERT( from_account.rc_manabar.last_update_time == now );
  resource_credits rc( _db );
  FC_ASSERT( !_db.is_in_control() || !rc.has_expired_delegation( from_account ), "Cannot delegate RC while processing of previous delegation has not finished." );
    // above is not strictly needed - we can handle new delegations during delayed undelegating just fine, however we want to discourage users
    // from using the scheme to temporarily "pump" amount of RC, also if it is not intentional they might be confused about fresh delegations
    // disappearing right after being formed (which might happen if we allow fresh delegations while previous were not yet cleared)
  int64_t delta_total = 0; // total amount of rc gained/delegated over the accounts

  for (const account_name_type& to:op.delegatees)
  {
    const account_object& to_account = _db.get_account( to );

    const rc_direct_delegation_object* delegation = _db.find<rc_direct_delegation_object, by_from_to>(
      boost::make_tuple( from_account.get_id(), to_account.get_id() ) );

    int64_t delta = op.max_rc;
    if( delegation == nullptr ) // delegation is being created
    {
      FC_ASSERT( op.max_rc != 0, "Cannot delegate 0 if you are creating new rc delegation" );
      _db.create<rc_direct_delegation_object>( from_account, to_account, op.max_rc );
    }
    else // delegation is being increased, decreased or deleted
    {
      FC_ASSERT( delegation->delegated_rc != uint64_t( op.max_rc ), "A delegation to that user with the same amount of RC already exists" );
      // In the case of an update, we want to allow the user to delegate more without having to un-delegate
      // Eg bob has 30 max rc out of 80 because 50 is already delegated to alice, he wants to increase his delegation to 70,
      // we count the 50 already delegated, so he actually only needs to delegate 20 more.
      delta -= delegation->delegated_rc;

      if( op.max_rc == 0 )
      {
        _db.remove( *delegation );
      }
      else
      {
        _db.modify<rc_direct_delegation_object>( *delegation, [&]( rc_direct_delegation_object& delegation )
        {
          delegation.delegated_rc = op.max_rc;
        } );
      }
    }
    rc.update_account_after_rc_delegation( to_account, now, delta );
    delta_total += delta;
  }

  // Get the minimum between the current RC and the maximum delegable RC, so that eve can't f.e. re-delegate delegated RC
  int64_t from_delegable_rc = std::min( from_account.get_maximum_rc( true ).value, from_account.rc_manabar.current_mana );
  // We do this assert at the end instead of in the loop because depending on the ordering of the accounts the delta can start off as from_delegable_rc < delta_total and then be valid as some delegations may get modified to take less rc
  FC_ASSERT( from_delegable_rc >= delta_total, "Account ${a} has insufficient RC (have ${h}, needs ${n})", ( "a", op.from )( "h", from_delegable_rc )( "n", delta_total ) );

  _db.modify< account_object >( from_account, [&]( account_object& acc )
  {
    // Do not give mana back when deleting/reducing rc delegation (note that regular delegations behave differently)
    if( delta_total > 0 )
    {
      acc.rc_manabar.current_mana -= delta_total;
      // since delta_total is not greater than from_delegable_rc which is not greater than current_mana, we know it can't dive into negative
    }
    acc.delegated_rc += delta_total;
    acc.last_max_rc = acc.get_maximum_rc();
  } );
}

} } // hive::chain

HIVE_DEFINE_OPERATION_TYPE( hive::chain::rc_custom_operation )

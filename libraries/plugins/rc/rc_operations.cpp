#include <hive/protocol/config.hpp>

#include <hive/plugins/rc/rc_config.hpp>
#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/rc/rc_operations.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/global_property_object.hpp>

#include <hive/chain/util/manabar.hpp>

#include <hive/protocol/operation_util_impl.hpp>

namespace hive { namespace plugins { namespace rc {

using namespace hive::chain;

void delegate_rc_operation::validate()const
{
  validate_account_name( from );
  validate_account_name( to );

  FC_ASSERT( max_rc >= 0, "amount of rc delegated cannot be negative" );
  FC_ASSERT( to != from, "cannot delegate rc to yourself" );
}

void delegate_rc_evaluator::do_apply( const delegate_rc_operation& op )
{
  if( !_db.has_hardfork( HIVE_HARDFORK_1_25 ) ) return;

  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();
  uint32_t now = gpo.time.sec_since_epoch();
  const rc_account_object& from_rc_account = _db.get< rc_account_object, by_name >( op.from );

  FC_ASSERT( from_rc_account.rc_manabar.last_update_time == now );

  const rc_direct_delegation_object* delegation = _db.find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( op.from, op.to ) );

  uint64_t already_delegated_rc = 0;
  if( !delegation )
  {
    FC_ASSERT( op.max_rc != 0, "Cannot delegate 0 if you are creating an rc delegation");
    const account_object* to_account = _db.find< account_object, by_name >( op.to );
    FC_ASSERT( to_account, "Account ${a} does not exist", ("a", op.to) );
  } else {
    already_delegated_rc = delegation->delegated_rc;
  }

  const rc_account_object& to_rc_account = _db.get< rc_account_object, by_name >( op.to );
  const account_object& from_account = _db.get< account_object, by_name >( op.from );
  // Get the minimum between the current RC and the maximum RC without received rc, so that eve can't re-delegate delegated RC
  int64_t from_delegateable_rc = std::min(get_maximum_rc( from_account, from_rc_account, true ), from_rc_account.rc_manabar.current_mana);

  // In the case of an update, we want to allow the user to delegate more without having to undelegate
  // eg bob has 30 max rc out of 80 because 50 is already delegated to alice, he wants to increase his delegation to 70, we count the 50 already delegated
  // so he actually only needs to delegate 20 more.
  int64_t delta = op.max_rc - already_delegated_rc;

  FC_ASSERT( from_delegateable_rc >= delta, "Account ${a} has insufficient RC (have ${h}, need ${n})",
             ("a", op.from)("h", from_delegateable_rc)("n", delta) );

  hive::chain::util::manabar to_manabar = to_rc_account.rc_manabar;
  hive::chain::util::manabar_params to_manabar_params;
  to_manabar_params.regen_time = HIVE_RC_REGEN_TIME;
  to_manabar_params.max_mana = to_rc_account.last_max_rc;
  to_manabar.regenerate_mana( to_manabar_params, now );
  to_manabar.current_mana = std::min( to_manabar.current_mana + delta, int64_t(0) );

  if (!delegation) {
    // delegation is being created
    _db.create< rc_direct_delegation_object >(op.from, op.to, op.max_rc);
  } else {
    // delegation is being increased, decreased or deleted
    if (op.max_rc == 0) {
      _db.remove( *delegation );
    } else {
      _db.modify<rc_direct_delegation_object>( *delegation, [&](rc_direct_delegation_object &delegation) {
        FC_ASSERT( delegation.delegated_rc + delta > 0 );
        delegation.delegated_rc += delta;
      });
    }
  }

  _db.modify< rc_account_object >( from_rc_account, [&]( rc_account_object& rca )
  {
    FC_ASSERT( get_maximum_rc( from_account, rca, true ) - delta >= 0 );
    // Do not give current rc back when deleting/reducing a delegation
    if (delta > 0) {
      FC_ASSERT( rca.rc_manabar.current_mana - delta >= 0 );
      rca.rc_manabar.current_mana -= delta;
    }
    rca.last_max_rc = get_maximum_rc( from_account, rca ) - delta;
    rca.delegated_rc += delta;
  });

  _db.modify< rc_account_object >( to_rc_account, [&]( rc_account_object& rca )
  {
    rca.rc_manabar = to_manabar;
    rca.last_max_rc = get_maximum_rc( from_account, rca ) + delta;
    rca.received_delegated_rc += delta;
  });
}


} } } // hive::plugins::rc

HIVE_DEFINE_OPERATION_TYPE( hive::plugins::rc::rc_plugin_operation )

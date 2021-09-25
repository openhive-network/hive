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

  FC_ASSERT( max_rc >= 0, "amount of rc delegated cannot be negative" ); // Cannot happen because max_rc is an uint but it doesn't hurt to pre-emptively have a validate if in the future it gets changed to int
  FC_ASSERT( to != from, "cannot delegate rc to yourself" );
}

void delegate_rc_evaluator::do_apply( const delegate_rc_operation& op )
{
  if( !_db.has_hardfork( HIVE_HARDFORK_1_25 ) ) return;

  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();
  uint32_t now = gpo.time.sec_since_epoch();
  const rc_account_object& from_rc_account = _db.get< rc_account_object, by_name >( op.from );
  const rc_account_object& to_rc_account = _db.get< rc_account_object, by_name >( op.to );
  FC_ASSERT( from_rc_account.rc_manabar.last_update_time == now );

  const account_object& from_account = _db.get< account_object, by_name >( op.from );
  const account_object* to_account = _db.find< account_object, by_name >( op.to );
  FC_ASSERT( to_account, "Account ${a} does not exist", ("a", op.to) );


  const rc_direct_delegation_object* delegation = _db.find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( from_account.get_id(), to_account->get_id() ) );

  uint64_t already_delegated_rc = 0;
  if( !delegation )
  {
    FC_ASSERT( op.max_rc != 0, "Cannot delegate 0 if you are creating the rc delegation");
  } else {
    FC_ASSERT( delegation->delegated_rc != op.max_rc, "A delegation to that user with the same amount of RC already exist" );
    already_delegated_rc = delegation->delegated_rc;
  }

  // Get the minimum between the current RC and the maximum RC without received rc, so that eve can't re-delegate delegated RC
  int64_t from_delegateable_rc = std::min(get_maximum_rc( from_account, from_rc_account, false ), from_rc_account.rc_manabar.current_mana);

  // In the case of an update, we want to allow the user to delegate more without having to un-delegate
  // Eg bob has 30 max rc out of 80 because 50 is already delegated to alice, he wants to increase his delegation to 70, we count the 50 already delegated
  // So he actually only needs to delegate 20 more.
  int64_t delta = op.max_rc - already_delegated_rc;

  FC_ASSERT( from_delegateable_rc >= delta, "Account ${a} has insufficient RC (have ${h}, need ${n})",
             ("a", op.from)("h", from_delegateable_rc)("n", delta) );

  if (!delegation) {
    // delegation is being created
    _db.create< rc_direct_delegation_object >(from_account.get_id(), to_account->get_id(), op.max_rc);
  } else {
    // delegation is being increased, decreased or deleted
    if (op.max_rc == 0) {
      _db.remove( *delegation );
    } else {
      _db.modify<rc_direct_delegation_object>( *delegation, [&](rc_direct_delegation_object &delegation) {
        FC_ASSERT( delegation.delegated_rc + delta > 0, "Delegation would result in negative delegated RC" );
        delegation.delegated_rc += delta;
      });
    }
  }

  _db.modify< rc_account_object >( from_rc_account, [&]( rc_account_object& rca )
  {
    FC_ASSERT( get_maximum_rc( from_account, rca, false ) - delta >= 0, "Delegation would result in negative maximum rc" );
    // Do not give current rc back when deleting/reducing a delegation
    if (delta > 0) {
      FC_ASSERT( rca.rc_manabar.current_mana - delta >= 0, "Delegation would result in negative rc" );
      rca.rc_manabar.current_mana -= delta;
    }
    rca.last_max_rc = get_maximum_rc( from_account, rca ) - delta;
    rca.delegated_rc += delta;
  });

  update_rc_account_after_delegation(_db, to_rc_account, to_account, now, delta);
}

} } } // hive::plugins::rc

HIVE_DEFINE_OPERATION_TYPE( hive::plugins::rc::rc_plugin_operation )

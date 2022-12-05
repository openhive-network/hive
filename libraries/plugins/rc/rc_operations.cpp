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
  uint32_t now = gpo.time.sec_since_epoch();
  const rc_account_object& from_rc_account = _db.get< rc_account_object, by_name >( op.from );
  FC_ASSERT( from_rc_account.rc_manabar.last_update_time == now );
  const account_object &from_account = _db.get<account_object, by_name>(op.from);
  int64_t delta_total = 0; // total amount of rc gained/delegated over the accounts

  for (const account_name_type& to:op.delegatees) {
    const rc_account_object &to_rc_account = _db.get<rc_account_object, by_name>(to);

    const account_object *to_account = _db.find<account_object, by_name>(to);
    FC_ASSERT(to_account, "Account ${a} does not exist", ("a", to));

    const rc_direct_delegation_object *delegation = _db.find<rc_direct_delegation_object, by_from_to>(
            boost::make_tuple(from_account.get_id(), to_account->get_id()));

    uint64_t already_delegated_rc = 0;
    if (!delegation) {
      FC_ASSERT(op.max_rc != 0, "Cannot delegate 0 if you are creating the rc delegation");
    } else {
      FC_ASSERT(delegation->delegated_rc != uint64_t(op.max_rc), "A delegation to that user with the same amount of RC already exist");
      already_delegated_rc = delegation->delegated_rc;
    }

    // In the case of an update, we want to allow the user to delegate more without having to un-delegate
    // Eg bob has 30 max rc out of 80 because 50 is already delegated to alice, he wants to increase his delegation to 70, we count the 50 already delegated
    // So he actually only needs to delegate 20 more.
    int64_t delta = op.max_rc - already_delegated_rc;
    delta_total += delta;

    if (!delegation) {
      // delegation is being created
      _db.create<rc_direct_delegation_object>(from_account, *to_account, op.max_rc);
    } else {
      // delegation is being increased, decreased or deleted
      if (op.max_rc == 0) {
        _db.remove(*delegation);
      } else {
        _db.modify<rc_direct_delegation_object>(*delegation, [&](rc_direct_delegation_object &delegation) {
          delegation.delegated_rc = op.max_rc;
        });
      }
    }
    update_rc_account_after_delegation(_db, to_rc_account, *to_account, now, delta);
  }

  // Get the minimum between the current RC and the maximum delegable RC, so that eve can't f.e. re-delegate delegated RC
  int64_t from_delegable_rc = std::min(get_maximum_rc(from_account, from_rc_account, true), from_rc_account.rc_manabar.current_mana);
  // We do this assert at the end instead of in the loop because depending on the ordering of the accounts the delta can start off as from_delegable_rc < delta_total and then be valid as some delegations may get modified to take less rc
  FC_ASSERT(from_delegable_rc >= delta_total, "Account ${a} has insufficient RC (have ${h}, needs ${n})", ("a", op.from)("h", from_delegable_rc)("n", delta_total));

  _db.modify< rc_account_object >( from_rc_account, [&]( rc_account_object& rca )
  {
    // Do not give mana back when deleting/reducing rc delegation (note that regular delegations behave differently)
    if (delta_total > 0)
      rca.rc_manabar.current_mana -= delta_total;
    rca.delegated_rc += delta_total;
    rca.last_max_rc = get_maximum_rc( from_account, rca );
  } );
}

} } } // hive::plugins::rc

HIVE_DEFINE_OPERATION_TYPE( hive::plugins::rc::rc_plugin_operation )

#include <hive/protocol/hive_custom_operations.hpp>

#include <hive/protocol/config.hpp>
#include <hive/protocol/operation_util_impl.hpp>
#include <hive/protocol/validation.hpp>

namespace hive { namespace protocol {

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

void follow_operation::validate()const
{
  FC_ASSERT( follower != following, "You cannot follow yourself" );
}

void reblog_operation::validate()const
{
  FC_ASSERT( account != author, "You cannot reblog your own content" );
}

} } // hive::protocol

#include <hive/protocol/hive_custom_operations.hpp>

#include <hive/protocol/config.hpp>
#include <hive/protocol/operation_util_impl.hpp>
#include <hive/protocol/validation.hpp>

#include <hive/protocol/hive_specialised_exceptions.hpp>


namespace hive { namespace protocol {

void delegate_rc_operation::validate()const
{
  validate_account_name( from );
  HIVE_PROTOCOL_VALIDATION_ASSERT(delegatees.size() != 0, "Must provide at least one account", ("subject", *this) );
  HIVE_PROTOCOL_VALIDATION_ASSERT(
    delegatees.size() <= HIVE_RC_MAX_ACCOUNTS_PER_DELEGATION_OP, 
    "Provided ${size} accounts, cannot delegate to more than ${max} accounts in one operation", 
    ("size", delegatees.size())("max", HIVE_RC_MAX_ACCOUNTS_PER_DELEGATION_OP)("subject", *this)
  );

  for(const account_name_type& delegatee:delegatees) {
    validate_account_name(delegatee);
    HIVE_PROTOCOL_VALIDATION_ASSERT( 
      delegatee != from, 
      "cannot delegate rc to yourself", 
      ("subject", *this)("account", delegatee) 
    );
  }

  HIVE_PROTOCOL_VALIDATION_ASSERT( max_rc >= 0, "amount of rc delegated cannot be negative", ("subject", *this) );
}

void follow_operation::validate()const
{
  HIVE_PROTOCOL_VALIDATION_ASSERT( follower != following, "You cannot follow yourself", ("subject", *this)("account", follower) );
}

void reblog_operation::validate()const
{
  HIVE_PROTOCOL_VALIDATION_ASSERT( account != author, "You cannot reblog your own content", ("subject", *this)("account", account) );
}

} } // hive::protocol

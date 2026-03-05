#include <hive/protocol/hive_custom_operations.hpp>

#include <hive/protocol/config.hpp>
#include <hive/protocol/operation_util_impl.hpp>
#include <hive/protocol/validation.hpp>

#include <hive/protocol/hive_specialised_exceptions.hpp>


namespace hive { namespace protocol {

void delegate_rc_operation::validate()const
{
  validate_account_name( from );
  HIVE_PROTOCOL_NUMBER_ASSERT(delegatees.size() != 0, "Must provide at least one account", ("subject", delegatees.size()) );
  HIVE_PROTOCOL_NUMBER_ASSERT(
    delegatees.size() <= HIVE_RC_MAX_ACCOUNTS_PER_DELEGATION_OP,
    "Provided ${size} accounts, cannot delegate to more than ${max} accounts in one operation",
    ("size", delegatees.size())("max", HIVE_RC_MAX_ACCOUNTS_PER_DELEGATION_OP)("subject", delegatees.size())
  );

  for(const account_name_type& delegatee:delegatees) {
    validate_account_name(delegatee);
    HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT(
      delegatee != from,
      "cannot delegate rc to yourself",
      ("subject", delegatee)("account", from)
    );
  }

  HIVE_PROTOCOL_NUMBER_ASSERT( max_rc >= 0, "amount of rc delegated cannot be negative", ("subject", max_rc) );
}

} } // hive::protocol

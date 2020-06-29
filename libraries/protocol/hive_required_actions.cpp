#include <hive/protocol/validation.hpp>
#include <hive/protocol/hive_required_actions.hpp>

namespace hive { namespace protocol {

#ifdef HIVE_ENABLE_SMT
void example_required_action::validate()const
{
  validate_account_name( account );
}

bool operator==( const example_required_action& lhs, const example_required_action& rhs )
{
  return lhs.account == rhs.account;
}
#endif

} } //hive::protocol

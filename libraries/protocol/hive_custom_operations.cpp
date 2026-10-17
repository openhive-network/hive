#include <hive/protocol/hive_custom_operations.hpp>

#include <hive/protocol/config.hpp>
#include <hive/protocol/operation_util_impl.hpp>
#include <hive/protocol/validation.hpp>

#include <hive/protocol/hive_specialised_exceptions.hpp>

#include <fc/reflect/variant.hpp>

#include <limits>


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

namespace fc
{

// generic int64_t conversion silently wraps unsigned values above int64 range into negative ones
void from_variant( const fc::variant& var, hive::protocol::delegate_rc_operation& vo )
{
  const variant_object& v_object = var.get_object();
  auto itr = v_object.find( "max_rc" );
  if( itr != v_object.end() )
  {
    const variant& max_rc = itr->value();
    const uint64_t max_int64 = static_cast< uint64_t >( std::numeric_limits< int64_t >::max() );
    HIVE_PROTOCOL_NUMBER_ASSERT( !max_rc.is_uint64() || max_rc.as_uint64() <= max_int64,
      "amount of rc delegated cannot exceed ${max}", ("max", max_int64)("subject", max_rc) );
  }
  fc::reflector< hive::protocol::delegate_rc_operation >::visit(
    from_variant_visitor< hive::protocol::delegate_rc_operation >( v_object, vo ) );
}

} // fc

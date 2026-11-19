#pragma once

#include <hive/protocol/hive_operations.hpp>
#include <fc/static_variant.hpp>

namespace hive { namespace protocol {

/**
  * Operations that can be embedded in custom_json_operation with id HIVE_RC_CUSTOM_OPERATION_ID.
  * Note: since HF29 delegate_rc_operation can also be used directly as regular operation.
  */
typedef fc::static_variant<
  delegate_rc_operation
> rc_custom_operation;

#define HIVE_RC_CUSTOM_OPERATION_ID "rc"

} } // hive::protocol

FC_REFLECT_TYPENAME( hive::protocol::rc_custom_operation )

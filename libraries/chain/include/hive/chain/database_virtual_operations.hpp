#pragma once

#include <hive/chain/database.hpp>
#include <hive/protocol/operations.hpp>

namespace hive { namespace chain {

using hive::protocol::operation;

/// Push a complete virtual operation (pre + post notifications)
void push_virtual_operation( database& db, const protocol::operation& op );

/// Push only pre-notification for a virtual operation
void pre_push_virtual_operation( database& db, const protocol::operation& op );

/// Push only post-notification for a virtual operation
void post_push_virtual_operation( database& db, const protocol::operation& op, const fc::optional<uint64_t>& op_in_trx = fc::optional<uint64_t>() );

/**
  * Push complete notification (pre + post) for regular operation that is not a direct part of transaction,
  * but was embedded in currently applied operation and became effective (f.e. delegate_rc_operation inside
  * custom_json_operation). Such operation is reported as if it was placed in transaction right after its
  * carrier - it is not virtual and it shares position of the carrier (op_in_trx), so repeated op_in_trx
  * of non-virtual operation tells that it is not a direct part of transaction.
  */
void push_interpreted_operation( database& db, const protocol::operation& op );

} } // hive::chain

#pragma once

#include <hive/chain/database.hpp>
#include <hive/protocol/operations.hpp>

namespace hive { namespace chain {

/// Push a complete virtual operation (pre + post notifications)
void push_virtual_operation( database& db, const protocol::operation& op );

/// Push only pre-notification for a virtual operation
void pre_push_virtual_operation( database& db, const protocol::operation& op );

/// Push only post-notification for a virtual operation
void post_push_virtual_operation( database& db, const protocol::operation& op, const fc::optional<uint64_t>& op_in_trx = fc::optional<uint64_t>() );

} } // hive::chain

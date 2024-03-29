#pragma once

#include <hive/protocol/types.hpp>

#include <fc/reflect/reflect.hpp>

namespace hive { namespace protocol {

struct signed_transaction;

} } // hive::protocol

namespace hive { namespace chain {

// returns account that is RC payer for given transaction (first operation decides)
hive::protocol::account_name_type get_resource_user( const hive::protocol::signed_transaction& tx );

} } // hive::chain

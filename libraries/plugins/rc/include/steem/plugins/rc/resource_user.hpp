#pragma once

#include <steem/protocol/types.hpp>

#include <fc/reflect/reflect.hpp>

namespace hive { namespace protocol {
struct signed_transaction;
} } // hive::protocol

namespace hive { namespace plugins { namespace rc {

using hive::protocol::account_name_type;
using hive::protocol::signed_transaction;

account_name_type get_resource_user( const signed_transaction& tx );

} } } // hive::plugins::rc

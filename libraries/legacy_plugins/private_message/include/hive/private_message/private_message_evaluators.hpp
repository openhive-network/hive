#pragma once

#include <hive/chain/evaluator.hpp>

#include <hive/private_message/private_message_operations.hpp>
#include <hive/private_message/private_message_plugin.hpp>

namespace hive { namespace private_message {

HIVE_DEFINE_PLUGIN_EVALUATOR( private_message_plugin, hive::private_message::private_message_plugin_operation, private_message )

} }

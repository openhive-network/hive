#pragma once

#include <hive/protocol/hive_required_actions.hpp>

#include <hive/chain/evaluator.hpp>

namespace hive { namespace chain {

using namespace hive::protocol;

#ifdef HIVE_ENABLE_SMT
HIVE_DEFINE_ACTION_EVALUATOR( example_required, required_automated_action )
#endif

} } //hive::chain

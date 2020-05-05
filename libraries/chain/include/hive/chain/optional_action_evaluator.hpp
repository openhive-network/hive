#pragma once

#include <hive/protocol/steem_optional_actions.hpp>

#include <hive/chain/evaluator.hpp>

namespace hive { namespace chain {

using namespace hive::protocol;

#ifdef IS_TEST_NET
HIVE_DEFINE_ACTION_EVALUATOR( example_optional, optional_automated_action )
#endif

} } //hive::chain

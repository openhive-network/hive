#pragma once

#include <hive/protocol/hive_operations.hpp>

#include <hive/chain/evaluator.hpp>
#include <hive/chain/evaluator_registry.hpp>

namespace hive { namespace chain {

using namespace hive::protocol;

// Registration functions for evaluators defined in split cpp files
void register_transfer_evaluators( evaluator_registry<operation>& registry );
void register_account_evaluators( evaluator_registry<operation>& registry );
void register_social_evaluators( evaluator_registry<operation>& registry );
void register_witness_evaluators( evaluator_registry<operation>& registry );
void register_dhf_evaluators( evaluator_registry<operation>& registry );
#ifdef HIVE_ENABLE_SMT
void register_smt_evaluators( evaluator_registry<operation>& registry );
#endif

} } // hive::chain

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

// SMT evaluators (defined in smt_evaluator.cpp)
#ifdef HIVE_ENABLE_SMT
HIVE_DEFINE_EVALUATOR( smt_setup )
HIVE_DEFINE_EVALUATOR( smt_setup_emissions )
HIVE_DEFINE_EVALUATOR( smt_set_setup_parameters )
HIVE_DEFINE_EVALUATOR( smt_set_runtime_parameters )
HIVE_DEFINE_EVALUATOR( smt_create )
HIVE_DEFINE_EVALUATOR( smt_contribute )
#endif

// DHF/Proposal evaluators (defined in dhf_evaluator.cpp)
HIVE_DEFINE_EVALUATOR( create_proposal )
HIVE_DEFINE_EVALUATOR( update_proposal )
HIVE_DEFINE_EVALUATOR( update_proposal_votes )
HIVE_DEFINE_EVALUATOR( remove_proposal )

} } // hive::chain

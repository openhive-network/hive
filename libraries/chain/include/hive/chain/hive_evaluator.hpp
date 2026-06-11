#pragma once

#include <hive/protocol/hive_operations.hpp>

#include <hive/chain/evaluator.hpp>
#include <hive/chain/evaluator_registry.hpp>

namespace hive { namespace chain {

using namespace hive::protocol;

class account_object;
class database;

// GCC 14's -Wdangling-reference heuristic gives false positives when a function
// returns a reference and is called with temporary arguments, even though the
// returned reference refers to a long-lived object (here a chainbase object) and
// not to any of those temporaries. [[gnu::no_dangling]] (GCC 14+) states that the
// result does not dangle. Guarded so other/older compilers just ignore it.
#if defined( __has_cpp_attribute ) && __has_cpp_attribute( gnu::no_dangling )
#  define HIVE_NO_DANGLING [[gnu::no_dangling]]
#else
#  define HIVE_NO_DANGLING
#endif

/// Helper function for creating accounts with proper recovery account handling (pre-HF11 uses "steem")
HIVE_NO_DANGLING
const account_object& create_account( database& db, const account_name_type& name, const public_key_type& key,
  const time_point_sec& _creation_time, const time_point_sec& _block_creation_time, bool mined, const HIVE_asset& fee_for_rc_adjustment,
  const account_object* recovery_account = nullptr, const VEST_asset& initial_delegation = VEST_asset( 0 ) );

// Registration functions for evaluators defined in split cpp files
void register_transfer_evaluators( evaluator_registry<operation>& registry );
void register_account_evaluators( evaluator_registry<operation>& registry );
void register_social_evaluators( evaluator_registry<operation>& registry );
void register_witness_evaluators( evaluator_registry<operation>& registry );
void register_dhf_evaluators( evaluator_registry<operation>& registry );

} } // hive::chain

#pragma once

#include <hive/protocol/operations.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

namespace hive { namespace chain {

/// The path the comment lookups made by the evaluator of the operation are counted under in the archive stats.
hive::utilities::benchmark_dumper::comment_lookup_path get_evaluator_lookup_path( const hive::protocol::operation& op );

} } // hive::chain

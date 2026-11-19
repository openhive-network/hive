#pragma once

#include <hive/protocol/version.hpp>

#include <fc/optional.hpp>
#include <fc/time.hpp>

#include <utility>
#include <vector>

namespace hive { namespace chain {

class database;

void update_witness_schedule( database& db );
void reset_virtual_schedule_time( database& db );

/// What one scheduled witness reports, as the version tally of the witness schedule reads it
struct witness_version_report
{
  protocol::version          running_version;
  protocol::hardfork_version hardfork_version_vote;
  fc::time_point_sec         hardfork_time_vote;
};

/// Result of tally_witness_versions
struct witness_version_tally
{
  /// highest version that at least the required number of witnesses run (this version or a newer one)
  fc::optional< protocol::version > majority_version;
  /// hardfork vote (version and time) cast by at least the required number of witnesses
  fc::optional< std::pair< protocol::hardfork_version, fc::time_point_sec > > hardfork_vote;
};

/// Counts the running versions and hardfork votes of the scheduled witnesses in one spelling: a major 0 counts as
/// major 1 (0.N and 1.N name the same release, see version.hpp; 0.0.0, never reported, stays 0.0.0) and the keys are
/// plain integers in a strict order, so the result depends neither on the order of the witnesses nor on the container
/// implementation. Versions in the result use the 1.N spelling.
witness_version_tally tally_witness_versions( const std::vector< witness_version_report >& reports, uint32_t required );

} }

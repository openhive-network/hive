#!/bin/bash
# Print the witness_tests cases of one CI plugin_test_witness shard, one
# --run_test= unit per line, for scripts/run_chain_tests_parallel.sh.
#
# Usage:
#   scripts/ci-helpers/witness_test_units.sh <plugin_test binary> <shard>
#
# The cases drive real-time block production, so a serial run of all of them
# outgrows one job's timeout. Each named shard below lists its cases; the shard
# "rest" holds every case no named shard lists, so a new case runs without
# being assigned. A listed case the binary does not have is an error, so a
# renamed case cannot drop out of the run unnoticed.
#
# Serial durations in the CI image (seconds), which the groups balance:
#   colony_basic   colony_basic_test 444
#   colony         colony_foreign_pending_test 114, colony_in_transit_test 132,
#                  colony_stuck_test 117, colony_no_workers_test 17,
#                  colony_queen_test 14, basic_queen_test 0; 394 together
#   rest           the 17 other cases, 474 together; the longest are
#                  inventory_limit_deferral_test 74 and four cases of ~65

set -euo pipefail

declare -A SHARDS=(
    [colony_basic]="colony_basic_test"
    [colony]="colony_foreign_pending_test colony_in_transit_test colony_stuck_test colony_no_workers_test basic_queen_test colony_queen_test"
)

BINARY="${1:?usage: $0 <plugin_test binary> <shard>}"
SHARD="${2:?usage: $0 <plugin_test binary> <shard>}"

mapfile -t CASES < <("$BINARY" --list_content 2>&1 | awk '
    /^[^ ]/ { suite = $1; sub(/[*:].*/, "", suite); next }
    suite == "witness_tests" && /^    [^ ]/ && /\*/ { name = $1; sub(/[*:].*/, "", name); print name }')
[ "${#CASES[@]}" -gt 0 ] || { echo "no witness_tests cases in $BINARY" >&2; exit 1; }

declare -A KNOWN=() ASSIGNED=()
for c in "${CASES[@]}"; do KNOWN[$c]=1; done
for s in "${!SHARDS[@]}"; do
    for c in ${SHARDS[$s]}; do
        [ -n "${KNOWN[$c]:-}" ] || { echo "shard $s lists $c, which $BINARY does not have" >&2; exit 1; }
        ASSIGNED[$c]=1
    done
done

if [ "$SHARD" = rest ]; then
    for c in "${CASES[@]}"; do
        [ -n "${ASSIGNED[$c]:-}" ] || echo "witness_tests/$c"
    done
elif [ -n "${SHARDS[$SHARD]:-}" ]; then
    for c in ${SHARDS[$SHARD]}; do echo "witness_tests/$c"; done
else
    echo "unknown shard $SHARD (known: ${!SHARDS[*]} rest)" >&2
    exit 1
fi

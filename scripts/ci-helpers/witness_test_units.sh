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
#   colony         colony_pending_backpressure_test ~114, colony_expiration_test ~117
#                  (estimated from the cases they replaced), colony_expiration_zero_refused_test
#                  and colony_expiration_too_long_refused_test ~0, colony_no_workers_test 17,
#                  colony_queen_test 14, basic_queen_test 0; ~262 together
#   rest           the 21 other cases (474 for the 17 timed when sharding), the longest
#                  inventory_limit_deferral_test 74 and four cases of ~65; the p2p
#                  inbound accept cases take about 10 and 15

set -euo pipefail

declare -A SHARDS=(
    [colony_basic]="colony_basic_test"
    [colony]="colony_pending_backpressure_test colony_expiration_test colony_expiration_zero_refused_test colony_expiration_too_long_refused_test colony_no_workers_test basic_queen_test colony_queen_test"
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

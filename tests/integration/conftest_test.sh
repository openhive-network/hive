#!/usr/bin/env bash
# Tests of conftest.sh's progress-based waits against stub containers (a busybox
# image: CONFTEST_TEST_IMAGE), with a short stall window. Needs a docker daemon,
# no hived and no peers. Run from anywhere:
#
#   tests/integration/conftest_test.sh
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=tests/integration/conftest.sh
. "$here/conftest.sh"
set +e
image="${CONFTEST_TEST_IMAGE:-busybox:latest}"
prefix="conftest-test-$$"
scratch="$(mktemp -d)"
trap 'docker ps -aq --filter "name=^$prefix-" | xargs -r docker rm -f > /dev/null 2>&1; rm -rf "$scratch"' EXIT
export HIVE_INTEGRATION_STALL_SECONDS=6 HIVE_INTEGRATION_POLL_SECONDS=1
failures=0

expect() {  # NAME EXPECTED ACTUAL
    if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$2], got [$3]"; failures=$((failures + 1)); fi
}

expect_between() {  # NAME LOW HIGH ACTUAL
    if [ "$4" -ge "$2" ] && [ "$4" -le "$3" ]; then echo "ok   $1 (${4}s)"; else echo "FAIL $1: ${4}s, expected ${2}-${3}s"; failures=$((failures + 1)); fi
}

stub() {  # NAME SCRIPT: a detached container running SCRIPT
    docker run -d --name "$prefix-$1" "$image" sh -c "$2" > /dev/null
    echo "$prefix-$1"
}

# Runs COMMAND...; sets rc, took (seconds) and out (its stdout and stderr).
measure() {
    local started
    started=$(date +%s)
    "$@" > "$scratch/out" 2>&1
    rc=$?
    took=$(( $(date +%s) - started ))
    out="$(cat "$scratch/out")"
}

# The seconds the failure message gives: monitor_container's own `docker stop`
# afterwards waits out busybox's PID 1, which ignores TERM.
reported() { grep -oE '[0-9]+s elapsed|ceiling after [0-9]+s' <<< "$out" | grep -oE '[0-9]+' | head -1; }

has() { grep -q -- "$1" <<< "$out" && echo yes || echo no; }

syncing='i=0; while :; do i=$((i + 1)); echo "Syncing Blockchain --- Got block: #$i time: x"; sleep 1; done'

# --- monitor_container ---------------------------------------------------------
c="$(stub silent 'sleep 1000')"
measure monitor_container "$c"
expect "a silent container fails" 1 "$rc"
expect "  as stalled" yes "$(has "stalled")"
expect "  not as the ceiling" no "$(has "ceiling after")"
expect_between "  within the stall window plus a poll" 6 8 "$(reported)"
expect "  and is stopped" exited "$(docker inspect --format '{{.State.Status}}' "$c")"

c="$(stub replaying 'i=0; while :; do i=$((i + 1000)); echo "   $i of 5000000 blocks = 0.1%"; sleep 1; done')"
HIVE_INTEGRATION_DEADLINE=$(( $(date +%s) + _STEP_CEILING_MARGIN + 14 )) measure monitor_container "$c"
expect "rising replay blocks run to the run's deadline" 124 "$rc"
expect "  failing as the ceiling" yes "$(has "reached the ceiling")"
expect "  not as stalled" no "$(has "stalled")"
expect_between "  past the stall window, at the deadline less the margin" 13 15 "$(reported)"
expect "  the container's log is shown" yes "$(has "3000 of 5000000 blocks")"

c="$(stub syncing "$syncing")"
measure monitor_container "$c" 10
expect "rising sync blocks run to the given cap" 124 "$rc"
expect "  failing as the ceiling" yes "$(has "reached the ceiling")"
expect_between "  at the cap" 10 11 "$(reported)"

c="$(stub noisy 'echo "Got block: #500"; while :; do echo "still here"; sleep 1; done')"
measure monitor_container "$c"
expect "output without a higher block is not progress" 1 "$rc"
expect "  failing as stalled" yes "$(has "stalled")"

c="$(stub talking 'while :; do echo "opening the state"; sleep 1; done')"
measure monitor_container "$c" 9
expect "before any block, new output is progress" yes "$(has "reached the ceiling")"

c="$(stub exiting 'echo "Got block: #1"; sleep 2; echo bye')"
measure monitor_container "$c"
expect "a container exiting 0 passes" 0 "$rc"
expect "  with its whole log shown" yes "$(has "bye")"

c="$(stub failed 'echo hi; sleep 2; exit 3')"
measure monitor_container "$c"
expect "a container exiting non-zero fails" 1 "$rc"
expect "  naming its exit code" yes "$(has "exited with code 3")"

# --- wait_for_container_block ---------------------------------------------------
# The head block reader replaced by a counter file: `step` added per read.
echo 100 > "$scratch/head"
get_container_head_block() {
    local h; h=$(( $(cat "$scratch/head") + ${step:-0} )); echo "$h" > "$scratch/head"; echo "$h"
}

step=0 measure wait_for_container_block stub 200 "Sync progress"
expect "an unchanging head block fails" 1 "$rc"
expect "  as stalled" yes "$(has "stalled")"
expect_between "  within the stall window plus a poll" 6 8 "$took"

echo 100 > "$scratch/head"
head="$(step=20 wait_for_container_block stub 200 "Sync progress" 2> /dev/null)"
expect "a rising head block reaches the target, which is all it prints" 200 "$head"

echo 100 > "$scratch/head"
step=1 measure wait_for_container_block stub 100000 "Sync progress" 8
expect "a head block rising short of the target fails at the cap" 124 "$rc"
expect "  as the ceiling" yes "$(has "reached the ceiling")"
unset -f get_container_head_block

# --- wait_for_chain_api_ready ----------------------------------------------------
c="$(stub api-silent 'sleep 1000')"
measure wait_for_chain_api_ready "$c"
expect "no API and no log output fails" 1 "$rc"
expect "  as stalled" yes "$(has "stalled")"

# --- the ceiling and run_step -----------------------------------------------------
HIVE_INTEGRATION_DEADLINE=$(( _conftest_started_at + 5000 )) measure _run_deadline
expect "a deadline past CI's 30 minutes is capped at them" "$(( _conftest_started_at + 1800 ))" "$out"
HIVE_INTEGRATION_DEADLINE=$(( _conftest_started_at + 900 )) measure _run_deadline
expect "an earlier deadline stands" "$(( _conftest_started_at + 900 ))" "$out"
HIVE_INTEGRATION_DEADLINE=soon measure _run_deadline
expect "a malformed deadline is refused" 1 "$rc"

c="$(stub script-ceiling "$syncing")"
measure bash -c "set -e; . '$here/conftest.sh'; run_step sync monitor_container '$c' 3; echo after"
expect "a script under set -e ends at a step's ceiling with timeout's status" 124 "$rc"
expect "  past no more of it" no "$(has "^after$")"
expect "  logging the step" yes "$(grep -qE '^step sync: [0-9]+s$' <<< "$out" && echo yes || echo no)"

measure run_step sync-stub false
expect "run_step keeps the command's status" 1 "$rc"
expect "  and logs the step's duration" yes "$(grep -qE '^step sync-stub: [0-9]+s$' <<< "$out" && echo yes || echo no)"

echo "$failures failure(s)"
[ "$failures" -eq 0 ]

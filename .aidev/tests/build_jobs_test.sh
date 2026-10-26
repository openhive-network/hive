#!/usr/bin/env bash
# Tests of .aidev/build-jobs.sh's job count over stubbed nproc, meminfo and
# cgroup files. Run from anywhere:
#
#   .aidev/tests/build_jobs_test.sh
set -uo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=.aidev/build-jobs.sh
. "$here/build-jobs.sh"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
failures=0

expect() {  # NAME EXPECTED ACTUAL
    if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$2], got [$3]"; failures=$((failures + 1)); fi
}

host_threads=32
nproc() { echo "$host_threads"; }
unset HIVE_BUILD_JOBS HIVE_BUILD_JOB_MEMORY_MB
export HIVE_MEMINFO="$scratch/meminfo" HIVE_CGROUP_MEMORY_MAX="$scratch/memory.max" \
    HIVE_CGROUP_MEMORY_CURRENT="$scratch/memory.current"

host_available_gb() {  # GB: the host's MemAvailable, no cgroup limit
    printf 'MemTotal:       131072000 kB\nMemFree:          1000000 kB\nMemAvailable:   %s kB\n' \
        "$(( $1 * 1024 * 1024 ))" > "$HIVE_MEMINFO"
    echo max > "$HIVE_CGROUP_MEMORY_MAX"
    echo 1073741824 > "$HIVE_CGROUP_MEMORY_CURRENT"
}

host_available_gb 100
expect "100 GB usable, nproc 32: nproc bounds it" 32 "$(build_jobs 2>/dev/null)"
host_available_gb 20
expect "20 GB usable: 2 GB per job" 10 "$(build_jobs 2>/dev/null)"
host_available_gb 1
expect "under 2 GB usable: one job" 1 "$(build_jobs 2>/dev/null)"
build_jobs > /dev/null 2>&1
expect "under 2 GB usable does not fail" 0 "$?"
host_available_gb 0
expect "no memory at all: one job" 1 "$(build_jobs 2>/dev/null)"

host_available_gb 20
expect "the log line" "build: 10 jobs (memory 20.0 GB usable / 2048 MB per job; nproc 32)" \
    "$(build_jobs 2>&1 >/dev/null)"
expect "exactly one log line" 1 "$(build_jobs 2>&1 >/dev/null | wc -l)"

host_available_gb 20
expect "HIVE_BUILD_JOB_MEMORY_MB" 20 "$(HIVE_BUILD_JOB_MEMORY_MB=1024 build_jobs 2>/dev/null)"
HIVE_BUILD_JOB_MEMORY_MB=lots build_jobs > /dev/null 2>&1
expect "HIVE_BUILD_JOB_MEMORY_MB not a number fails" 1 "$?"

# A container limited to 16 GB, 6 GB of it used, on a host with 100 GB available.
host_available_gb 100
echo $(( 16 * 1024 * 1024 * 1024 )) > "$HIVE_CGROUP_MEMORY_MAX"
echo $(( 6 * 1024 * 1024 * 1024 )) > "$HIVE_CGROUP_MEMORY_CURRENT"
expect "cgroup limit: its remaining memory, not MemAvailable" 5 "$(build_jobs 2>/dev/null)"
echo $(( 17 * 1024 * 1024 * 1024 )) > "$HIVE_CGROUP_MEMORY_CURRENT"
expect "cgroup usage above its limit: one job" 1 "$(build_jobs 2>/dev/null)"
rm "$HIVE_CGROUP_MEMORY_MAX" "$HIVE_CGROUP_MEMORY_CURRENT"
expect "no cgroup files: MemAvailable" 32 "$(build_jobs 2>/dev/null)"

host_threads=4
expect "nproc 4, 100 GB usable" 4 "$(build_jobs 2>/dev/null)"
host_threads=32

host_available_gb 1
expect "HIVE_BUILD_JOBS=4 overrides the memory" 4 "$(HIVE_BUILD_JOBS=4 build_jobs 2>/dev/null)"
expect "HIVE_BUILD_JOBS log line" "build: 4 jobs (HIVE_BUILD_JOBS; nproc 32)" \
    "$(HIVE_BUILD_JOBS=4 build_jobs 2>&1 >/dev/null)"
HIVE_BUILD_JOBS=0 build_jobs > /dev/null 2>&1
expect "HIVE_BUILD_JOBS=0 fails" 1 "$?"

: > "$HIVE_MEMINFO"
build_jobs > /dev/null 2>&1
expect "no MemAvailable and no cgroup limit fails" 1 "$?"

ninja() { echo "ninja $*"; }
host_available_gb 20
expect "build_ninja: -j from memory, -l nproc" "ninja -j 10 -l 32 hived cli_wallet" "$(build_ninja hived cli_wallet 2>/dev/null)"
expect "build_ninja: HIVE_BUILD_JOBS=4" "ninja -j 4 -l 32 chain_test" "$(HIVE_BUILD_JOBS=4 build_ninja chain_test 2>/dev/null)"
expect "build_ninja: one log line" 1 "$(build_ninja chain_test 2>&1 >/dev/null | grep -c '^build: ')"
HIVE_BUILD_JOBS=x build_ninja chain_test > "$scratch/ninja.out" 2>/dev/null
expect "build_ninja: a bad HIVE_BUILD_JOBS runs no ninja" "1|" "$?|$(cat "$scratch/ninja.out")"

[ "$failures" -eq 0 ] && echo "all passed" || { echo "$failures failed"; exit 1; }

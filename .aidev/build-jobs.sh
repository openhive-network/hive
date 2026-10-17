# shellcheck shell=bash
# The build's parallelism, sourced by run-checks.sh and tested by
# .aidev/tests/build_jobs_test.sh. A hive compile unit takes about 1.2 GB on
# average and up to 2.8 GB, so ninja's nproc default lets one build on a large
# host take a third of its memory, and a few concurrent builds OOM it: the job
# count follows the memory this build can actually use.
#
#   HIVE_BUILD_JOBS            the job count, overriding the computed one
#   HIVE_BUILD_JOB_MEMORY_MB   memory planned per job (default 2048)
#
# The files read are overridable for the tests: HIVE_CGROUP_MEMORY_MAX,
# HIVE_CGROUP_MEMORY_CURRENT, HIVE_MEMINFO.

# Memory this build may still take, in MB: the cgroup's remaining memory when it
# has a limit, else the host's MemAvailable.
usable_memory_mb() {
    local max_file="${HIVE_CGROUP_MEMORY_MAX:-/sys/fs/cgroup/memory.max}"
    local current_file="${HIVE_CGROUP_MEMORY_CURRENT:-/sys/fs/cgroup/memory.current}"
    local max current
    max="$(cat "$max_file" 2>/dev/null)"
    if [[ "$max" =~ ^[0-9]+$ ]] && current="$(cat "$current_file" 2>/dev/null)" && [[ "$current" =~ ^[0-9]+$ ]]; then
        echo $(( max > current ? (max - current) / 1048576 : 0 ))
        return
    fi
    awk '$1 == "MemAvailable:" { print int($2 / 1024); found = 1; exit } END { exit !found }' \
        "${HIVE_MEMINFO:-/proc/meminfo}"
}

# Prints the job count; logs the decision on stderr.
build_jobs() {
    local cpus per_job usable jobs
    cpus="$(nproc)"
    if [ -n "${HIVE_BUILD_JOBS:-}" ]; then
        [[ "$HIVE_BUILD_JOBS" =~ ^[1-9][0-9]*$ ]] || { echo "HIVE_BUILD_JOBS=$HIVE_BUILD_JOBS is not a positive integer" >&2; return 1; }
        echo "build: $HIVE_BUILD_JOBS jobs (HIVE_BUILD_JOBS; nproc $cpus)" >&2
        echo "$HIVE_BUILD_JOBS"
        return
    fi
    per_job="${HIVE_BUILD_JOB_MEMORY_MB:-2048}"
    [[ "$per_job" =~ ^[1-9][0-9]*$ ]] || { echo "HIVE_BUILD_JOB_MEMORY_MB=$per_job is not a positive integer" >&2; return 1; }
    usable="$(usable_memory_mb)" || { echo "build: no memory figure (cgroup or MemAvailable)" >&2; return 1; }
    jobs=$(( usable / per_job ))
    [ "$jobs" -le "$cpus" ] || jobs="$cpus"
    [ "$jobs" -ge 1 ] || jobs=1
    echo "build: $jobs jobs (memory $(awk -v mb="$usable" 'BEGIN { printf "%.1f", mb / 1024 }') GB usable / $per_job MB per job; nproc $cpus)" >&2
    echo "$jobs"
}

# ninja TARGET..., at build_jobs' parallelism, starting no job while the load
# average is above the host's thread count.
build_ninja() {
    local jobs
    jobs="$(build_jobs)" || return 1
    ninja -j "$jobs" -l "$(nproc)" "$@"
}

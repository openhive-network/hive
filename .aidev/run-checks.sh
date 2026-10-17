#!/usr/bin/env bash
# The step functions are called indirectly, through `step`.
# shellcheck disable=SC2317,SC2329
# The checks AIDEV's verification slots run (.aidev/project.yaml), as one junit
# report per suite: each named step is a test case, its log the failure body.
#
#   .aidev/run-checks.sh <suite> <step>[,<step>...]...
#
# Steps run in order; steps joined by commas run at the same time, sharing the
# HIVE_TEST_JOBS test processes (default: min(8, nproc)) evenly between them;
# when one of them ends, its share goes to those still running.
#
#   - build           configure and build the testnet chain_test and plugin_test,
#                     exactly as CI's testnet_node_build compiles them, and
#                     assertion_id_verifier, which fails when two FC_ASSERT
#                     conditions in protocol or chain sources share an assertion ID
#   - chain-smoke     chain_test basic_tests
#   - chain-tests     every chain_test suite, in parallel (scripts/run_chain_tests_parallel.sh)
#   - plugin-api      plugin_test without witness_tests (CI's plugin_test_api)
#   - plugin-tests    all of plugin_test (CI's plugin_test_api + plugin_test_witness), in
#                     parallel: the suites other than witness_tests as one process,
#                     each witness_tests case as its own
#   - integration     the tests/integration scripts the diff selects (.aidev/integration.sh),
#                     each against a hived image built from the checkout, one junit case
#                     per script; off unless HIVE_AIDEV_INTEGRATION=1, and needs a docker
#                     daemon (a `docker: true` suite)
#   - hived-testnet, hived-mainnet
#                     hived and the tools its image carries, into <report dir>/bin
#                     (what `integration` runs in the checks' image to build an image)
#
# The build's ninja runs as many jobs as the memory left to it allows, 2 GB each
# (.aidev/build-jobs.sh); HIVE_BUILD_JOBS overrides that.
#
# Compiler cache: the build runs from /home/hived/source into /home/hived/build
# with CI's cmake arguments, because sccache keys include absolute paths, so the
# checks hit what CI's builds put in its Redis cache (reached through the stack's
# `sccache` relay, .aidev/sccache-relay.compose.yml). When the relay or Redis is
# unreachable, sccache uses a local cache and the build is cold.
#
# Reports go to test-results/<suite>/: junit.xml (one case per step) plus each
# test binary's own junit report, and timings.tsv, each step's wall time.
#
# Each test unit runs under its own timeout (scripts/run_chain_tests_parallel.sh),
# widened under AIDEV's adaptive timeouts; see .aidev/README.md.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1

suite="${1:?usage: $0 <suite> <step>...}"; shift
out="$PWD/test-results/$suite"
rm -rf "$out"; mkdir -p "$out"
cases="$out/cases.tsv"; : > "$cases"
timings="$out/timings.tsv"; printf 'step\tseconds\texit\n' > "$timings"
date +%s > "$out/started-at"  # the slot's start, for a later suite's time budget
cpus="$(nproc)"
test_jobs="${HIVE_TEST_JOBS:-$(( cpus < 8 ? cpus : 8 ))}"
# What a step's tests run in parallel: a number, or a file holding one that
# concurrently() rewrites as the steps beside it end.
test_slots="$test_jobs"

# The steps concurrently() runs, by PID, each also the ID of the session it
# leads, and the directory of their parallelism files.
declare -A step_of=()
slots=""

# The slot wall terminates only this script: take the steps running in the
# background, and the tests they started, down with it. A step is killed by its
# session, not its process group: GNU parallel and timeout give their jobs
# groups of their own, and whatever a step starts after the TERM is still in it.
stop_steps() {
    trap - INT TERM EXIT
    local sid
    if [ "${#step_of[@]}" -gt 0 ]; then
        echo "stopping the running steps" >&2
        for sid in "${!step_of[@]}"; do pkill -TERM -s "$sid"; done
        sleep 2
        for sid in "${!step_of[@]}"; do pkill -KILL -s "$sid"; done
    fi
    [ -z "$slots" ] || rm -rf "$slots"
    return 0
}
trap 'stop_steps; exit 130' INT
trap 'stop_steps; exit 143' TERM
trap stop_steps EXIT

# fc's and utilities' CMake read each `.git` file's relative `gitdir:` and join it
# to the path they were given without resolving symlinks, so from $src a checkout
# whose git directories live outside it (a worktree, submodules of one) points at
# paths that don't exist. Link each such path, inside the container, to the real
# git directory, outermost first so a nested submodule's path runs through its
# parent's link; the checkout is not touched.
link_gitdirs() {
    local f dir rel real seen
    while IFS= read -r f; do
        dir="$(dirname "$f")"
        rel="$(sed -n 's/^gitdir: //p' "$PWD/$f")"
        case "$rel" in ""|/*) continue ;; esac
        real="$(cd "$PWD/$dir" && realpath -m "$rel")"
        seen="$(realpath -m -s "$src/$dir/$rel")"
        [ "$real" = "$seen" ] || [ -e "$seen" ] && continue
        mkdir -p "$(dirname "$seen")" 2>/dev/null && ln -s "$real" "$seen" 2>/dev/null || return 1
    done < <(find . -maxdepth 6 -name .git -type f -not -path './build/*' -printf '%d %P\n' | sort -n | cut -d' ' -f2-)
}

# $src mirrors the checkout entry by entry. Its `.git` points at a container-local
# directory holding HEAD as a detached hash: CMake reads HEAD's ref straight from
# the git directory (a worktree on a branch keeps its refs elsewhere) and joins
# the `gitdir:` to the directory even when it is absolute.
mirror_source() {
    local entry
    mkdir "$src" || return 1
    for entry in "$PWD"/* "$PWD"/.[!.]*; do
        [ -e "$entry" ] && [ "${entry##*/}" != .git ] && ln -s "$entry" "$src/${entry##*/}"
    done
    mkdir -p /home/hived/.source-git && git rev-parse HEAD > /home/hived/.source-git/HEAD || return 1
    echo "gitdir: ../.source-git" > "$src/.git"  # relative: fc's CMake joins it to the directory
}

src=/home/hived/source
bld=/home/hived/build
if ! { [ -w /home/hived ] && [ ! -e "$src" ] && mirror_source && link_gitdirs; }; then
    echo "cannot use CI's paths under /home/hived (uid $(id -u)); building in $PWD/build without CI's cache keys" >&2
    src="$PWD"; bld="$PWD/build"
fi

# shellcheck source=.aidev/build-jobs.sh
. .aidev/build-jobs.sh
# shellcheck source=.aidev/integration.sh
. .aidev/integration.sh

status=0
# A step function returns this to be recorded skipped, the last line of its log
# naming why.
step_skipped=77
# record CASES_FILE NAME RC SECONDS LOG
record() {
    if [ "$3" -eq 0 ]; then
        printf 'case\t%s\tpass\t%s\t\n' "$2" "$4" >> "$1"
    elif [ "$3" -eq "$step_skipped" ]; then
        printf 'case\t%s\tskip\t%s\t%s\n' "$2" "$4" "$(tail -1 "$5")" >> "$1"
    else
        printf 'case\t%s\tfail\t%s\texit %s\t%s\n' "$2" "$4" "$3" "$5" >> "$1"
    fi
}

step() {
    local name="$1"; shift
    local log="$out/$name.log" t0=$SECONDS rc=0 took
    echo "== $name" >&2
    "$@" > "$log" 2>&1 < /dev/null || rc=$?
    took=$((SECONDS - t0))
    [ "$rc" -eq 0 ] || tail -40 "$log" >&2
    echo "== $name: exit $rc after ${took}s" >&2
    record "$cases" "$name" "$rc" "$took" "$log"
    printf '%s\t%s\t%s\n' "$name" "$took" "$rc" >> "$timings"
    return "$rc"
}

# CI's cache through the stack's relay, else at HIVE_SCCACHE_REDIS (CI's own
# address, which a build on the host's network may reach), else a local one.
sccache_backend() {
    local url
    if [ -n "${SCCACHE_REDIS:-}" ]; then return; fi
    for url in redis://sccache:6379 ${HIVE_SCCACHE_REDIS:-}; do
        if redis_answers "$url"; then
            export SCCACHE_REDIS="$url"
            echo "sccache: CI cache at $SCCACHE_REDIS"
            return
        fi
    done
    export SCCACHE_DIR="${SCCACHE_DIR:-$PWD/.sccache}"
    echo "sccache: no CI cache answers, local cache in $SCCACHE_DIR (cold build)"
}

submodules_checked_out() {
    local m
    for m in libraries/fc libraries/appbase libraries/vendor/rocksdb libraries/vendor/zstd \
        libraries/chain/compression_dictionaries; do
        [ -n "$(ls -A "$src/$m" 2>/dev/null)" ] || { echo "submodule $m is not checked out"; return 1; }
    done
}

configure() {  # TESTNET (ON|OFF): CI's cmake arguments, so sccache shares CI's cache
    submodules_checked_out || return 1
    mkdir -p "$bld" && cd "$bld" || return 1
    cmake -DCMAKE_BUILD_TYPE=Release -GNinja \
        -DBUILD_HIVE_TESTNET="$1" -DHIVE_CONVERTER_BUILD=OFF -DHIVE_LINT=OFF "$src"
}

build() (  # a subshell: the `cd` below must not move the steps that follow
    sccache_backend
    configure ON || return 1
    local rc=0 started="$bld/.ninja-started"
    touch "$started" || return 1
    build_ninja chain_test plugin_test assertion_id_verifier || rc=$?
    sccache --show-stats || true
    [ "$rc" -eq 0 ] || duplicate_assertion_ids "$bld/generated_assert_id" "$started"
    return "$rc"
)

# The generated headers define one `uint64_t assertion_<hash>` per FC_ASSERT,
# commented with its source file and condition; the hash is of the condition
# text, so two asserts with the same condition redefine one name. Printed last,
# so the step's log tail names them whatever else the build printed. Only headers
# this build generated count: an older one may describe sources since fixed.
duplicate_assertion_ids() {  # GENERATED_ASSERT_ID_DIR SINCE_FILE
    local h headers=()
    for h in "$1"/*_assertion_hashes_verifier.hpp; do
        [ "$h" -nt "$2" ] && headers+=("$h")
    done
    [ "${#headers[@]}" -gt 0 ] || return 0
    awk '
        match($0, /assertion_[0-9]+ =/) {
            id = substr($0, RSTART, RLENGTH - 2)
            n[id]++; where[id] = where[id] sprintf("\n  %s:%d: %s", FILENAME, FNR, $0)
        }
        END { for (id in n) if (n[id] > 1)
                  printf "duplicate assertion ID %s: FC_ASSERT conditions must differ (the ID hashes the condition text)%s\n", id, where[id] }' "${headers[@]}" >&2
}

# The binaries the Dockerfile's build stage installs that an integration script
# or the image's entrypoint runs, as the instance stage takes them: <report dir>/bin.
hived_binaries() (  # TESTNET (ON|OFF)
    sccache_backend
    configure "$1" || return 1
    build_ninja hived cli_wallet shared_memory_file_util || return 1
    sccache --show-stats || true
    mkdir -p "$out/bin" && cp programs/hived/hived programs/cli_wallet/cli_wallet \
        programs/util/shared_memory_file_util "$out/bin/"
)

bin() {  # NAME: the built test binary
    find "$bld/tests" -name "$1" -type f -perm -u+x -print -quit 2>/dev/null
}

boost_test() {  # NAME BINARY [--run_test=...]
    local name="$1" exe; exe="$(bin "$2")"; shift 2
    [ -n "$exe" ] || { echo "no built $name binary under $bld/tests (did the build step run?)"; return 1; }
    "$exe" "$@" --log_format=JUNIT --log_sink="$out/$name.xml" --log_level=error --report_level=short
}

chain_tests() {
    local exe; exe="$(bin chain_test)"
    [ -n "$exe" ] || { echo "no built chain_test under $bld/tests"; return 1; }
    local rc=0
    scripts/run_chain_tests_parallel.sh "$exe" "$test_slots" || rc=$?
    cp "$(dirname "$exe")"/chain_test_results.xml "$out/chain-tests.xml" 2>/dev/null || true
    return "$rc"
}

# plugin_test's --run_test units: the suites other than witness_tests stay one
# process, because snapshots_tests fails without the suites before it; each
# witness_tests case (real-time block production, each in its own HIVE_TEMPDIR,
# p2p on no fixed port) is its own. colony_basic_test alone takes ~7.5 min, the
# rest together less, so it starts first and bounds the step. Its units' timeout
# floor is 900 s: chain_test's 600 s leaves colony_basic_test too little headroom.
plugin_units() {  # BINARY
    "$1" --list_content 2>&1 | awk -v first=colony_basic_test '
        /^[^ ]/ { suite = $1; sub(/[*:].*/, "", suite)
                  if (suite != "witness_tests" && /\*/) others = 1; next }
        suite == "witness_tests" && /^    [^ ]/ && /\*/ { name = $1; sub(/[*:].*/, "", name); n++; witness[n] = name }
        END { for (i = 1; i <= n; i++) if (witness[i] == first) print "witness_tests/" first
              if (others) print "!witness_tests"
              for (i = 1; i <= n; i++) if (witness[i] != first) print "witness_tests/" witness[i] }'
}

plugin_tests() {
    local exe; exe="$(bin plugin_test)"
    [ -n "$exe" ] || { echo "no built plugin_test under $bld/tests"; return 1; }
    local units="$out/plugin-tests-units.txt" rc=0
    plugin_units "$exe" > "$units" || return 1
    scripts/run_chain_tests_parallel.sh "$exe" "$test_slots" "$units" 900 || rc=$?
    cp "$(dirname "$exe")"/plugin_test_results.xml "$out/plugin-tests.xml" 2>/dev/null || true
    return "$rc"
}

run() {  # STEP
    case "$1" in
        build) step build build ;;
        chain-smoke) step chain-smoke boost_test chain-smoke chain_test --run_test=basic_tests ;;
        chain-tests) step chain-tests chain_tests ;;
        plugin-api) step plugin-api boost_test plugin-api plugin_test '--run_test=!witness_tests' ;;
        plugin-tests) step plugin-tests plugin_tests ;;
        integration) integration ;;
        hived-testnet) step hived-testnet hived_binaries ON ;;
        hived-mainnet) step hived-mainnet hived_binaries OFF ;;
    esac
}

share_slots() {  # DIR STEP...: $test_jobs split between the steps, one file each
    local dir="$1" share extra n s; shift
    [ "$#" -gt 0 ] || return 0
    share=$(( test_jobs / $# )); extra=$(( test_jobs % $# ))
    for s in "$@"; do
        n=$(( share + (extra > 0) )); [ "$n" -ge 1 ] || n=1
        extra=$(( extra > 0 ? extra - 1 : 0 ))
        echo "$n" > "$dir/$s.new" && mv "$dir/$s.new" "$dir/$s"
    done
}

# Returns step_skipped when every step was skipped, else 1 when one failed.
# Polls rather than `wait -n`: bash 4.4's `wait -n` waits only for a job that
# ends after it is called, so a step that ended just before would keep its share.
# Each step runs in a session of its own, so a fresh bash given this script's
# functions and settings; a background job of a script leads no process group,
# so setsid makes the job itself the session leader, without forking.
concurrently() {  # STEP...: sharing $test_jobs, a step's share handed on when it ends
    local s pid rc=0 ran=0 step_rc running ended
    step_of=()
    slots="$(mktemp -d)" || return 1
    share_slots "$slots" "$@"
    for s in "$@"; do
        setsid "$BASH" -c "set -uo pipefail; $(declare -p out cases timings src bld step_skipped); $(declare -f)"'
            test_slots="$1"; run "$2"' step "$slots/$s" "$s" < /dev/null &
        step_of[$!]="$s"
    done
    while [ "${#step_of[@]}" -gt 0 ]; do
        sleep 1
        running=" $(jobs -pr | tr '\n' ' ') " ended=0
        for pid in "${!step_of[@]}"; do
            [[ "$running" == *" $pid "* ]] && continue
            wait "$pid"; step_rc=$?
            [ "$step_rc" -eq "$step_skipped" ] || ran=1
            [ "$step_rc" -eq 0 ] || [ "$step_rc" -eq "$step_skipped" ] || rc=1
            unset "step_of[$pid]"; ended=1
        done
        [ "$ended" -eq 1 ] && [ "${#step_of[@]}" -gt 0 ] || continue
        share_slots "$slots" "${step_of[@]}"
        for s in "${step_of[@]}"; do echo "== $s: now $(cat "$slots/$s") test processes" >&2; done
    done
    rm -rf "$slots"; slots=""
    [ "$ran" -eq 1 ] || return "$step_skipped"
    return "$rc"
}

for group in "$@"; do
    IFS=, read -ra group_steps <<< "$group"
    for s in "${group_steps[@]}"; do
        case "$s" in
            build|chain-smoke|chain-tests|plugin-api|plugin-tests|integration|hived-testnet|hived-mainnet) ;;
            *) echo "unknown step: $s" >&2; exit 2 ;;
        esac
    done
done

# A suite whose steps were all skipped ran nothing: it exits step_skipped, its
# last line `skipped: ` and the skipped cases' reasons, rather than passing.
ran=0
for group in "$@"; do
    IFS=, read -ra group_steps <<< "$group"
    concurrently "${group_steps[@]}"; rc=$?
    [ "$rc" -eq "$step_skipped" ] || ran=1
    [ "$rc" -eq 0 ] || [ "$rc" -eq "$step_skipped" ] || status=1
done
[ "$ran" -eq 1 ] || status="$step_skipped"
printf 'total\t%s\t%s\n' "$SECONDS" "$status" >> "$timings"
echo "== wall time per step (seconds):" >&2
cat "$timings" >&2
skipped="$(awk -F'\t' '$1 == "case" && $3 == "skip" { printf "%s%s: %s", sep, $2, $5; sep = "; " }' "$cases")"
[ -z "$skipped" ] || { echo "== skipped:"; awk -F'\t' '$1 == "case" && $3 == "skip" { print "  " $2 ": " $5 }' "$cases"; } >&2
python3 .aidev/junit_cases.py "$out/junit.xml" "$suite" "$cases"
[ "$status" -ne "$step_skipped" ] || echo "skipped: $skipped"
exit "$status"

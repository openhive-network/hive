#!/usr/bin/env bash
# The step functions are called indirectly, through `step`.
# shellcheck disable=SC2317,SC2329
# The checks AIDEV's verification slots run (.aidev/project.yaml), as one junit
# report per suite: each named step is a test case, its log the failure body.
#
#   .aidev/run-checks.sh <suite> <step>...
#
#   - build           configure and build the testnet chain_test and plugin_test,
#                     exactly as CI's testnet_node_build compiles them
#   - chain-smoke     chain_test basic_tests
#   - chain-tests     every chain_test suite, in parallel (scripts/run_chain_tests_parallel.sh)
#   - plugin-api      plugin_test without witness_tests (CI's plugin_test_api)
#   - plugin-tests    all of plugin_test (CI's plugin_test_api + plugin_test_witness)
#
# Compiler cache: the build runs from /home/hived/source into /home/hived/build
# with CI's cmake arguments, because sccache keys include absolute paths, so the
# checks hit what CI's builds put in its Redis cache (reached through the stack's
# `sccache` relay, .aidev/sccache-relay.compose.yml). When the relay or Redis is
# unreachable, sccache uses a local cache and the build is cold.
#
# Reports go to test-results/<suite>/: junit.xml (one case per step) plus each
# test binary's own junit report.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1

suite="${1:?usage: $0 <suite> <step>...}"; shift
out="$PWD/test-results/$suite"
rm -rf "$out"; mkdir -p "$out"
cases="$out/cases.tsv"; : > "$cases"

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

status=0
# record CASES_FILE NAME RC SECONDS LOG
record() {
    if [ "$3" -eq 0 ]; then
        printf 'case\t%s\tpass\t%s\t\n' "$2" "$4" >> "$1"
    else
        printf 'case\t%s\tfail\t%s\texit %s\t%s\n' "$2" "$4" "$3" "$5" >> "$1"
    fi
}

step() {
    local name="$1"; shift
    local log="$out/$name.log" t0=$SECONDS rc=0
    echo "== $name" >&2
    "$@" > "$log" 2>&1 < /dev/null || rc=$?
    [ "$rc" -eq 0 ] || { status=1; tail -40 "$log" >&2; }
    record "$cases" "$name" "$rc" "$((SECONDS - t0))" "$log"
}

sccache_backend() {
    if [ -n "${SCCACHE_REDIS:-}" ]; then return; fi
    if (exec 3<>/dev/tcp/sccache/6379 && printf 'PING\r\n' >&3 && timeout 3 head -c5 <&3 | grep -q PONG) 2>/dev/null; then
        export SCCACHE_REDIS=redis://sccache:6379
        echo "sccache: CI cache through the relay ($SCCACHE_REDIS)"
    else
        export SCCACHE_DIR="${SCCACHE_DIR:-$PWD/.sccache}"
        echo "sccache: relay unreachable, local cache in $SCCACHE_DIR (cold build)"
    fi
}

build() {
    sccache_backend
    local m
    for m in libraries/fc libraries/appbase libraries/vendor/rocksdb libraries/vendor/zstd \
        libraries/chain/compression_dictionaries; do
        [ -n "$(ls -A "$src/$m" 2>/dev/null)" ] || { echo "submodule $m is not checked out"; return 1; }
    done
    mkdir -p "$bld" && cd "$bld" || return 1
    cmake -DCMAKE_BUILD_TYPE=Release -GNinja \
        -DBUILD_HIVE_TESTNET=ON -DHIVE_CONVERTER_BUILD=OFF -DHIVE_LINT=OFF "$src" || return 1
    local rc=0
    ninja -j "${HIVE_BUILD_JOBS:-$(nproc)}" chain_test plugin_test || rc=$?
    sccache --show-stats || true
    return "$rc"
}

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
    scripts/run_chain_tests_parallel.sh "$exe" "${HIVE_TEST_JOBS:-6}" || rc=$?
    cp "$(dirname "$exe")"/chain_test_results.xml "$out/chain-tests.xml" 2>/dev/null || true
    return "$rc"
}


for s in "$@"; do
    case "$s" in
        build) step build build ;;
        chain-smoke) step chain-smoke boost_test chain-smoke chain_test --run_test=basic_tests ;;
        chain-tests) step chain-tests chain_tests ;;
        plugin-api) step plugin-api boost_test plugin-api plugin_test '--run_test=!witness_tests' ;;
        plugin-tests) step plugin-tests boost_test plugin-tests plugin_test ;;
        *) echo "unknown step: $s" >&2; exit 2 ;;
    esac
done
python3 .aidev/junit_cases.py "$out/junit.xml" "$suite" "$cases"
exit "$status"

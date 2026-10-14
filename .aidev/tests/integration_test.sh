#!/usr/bin/env bash
# Tests of .aidev/integration.sh's decisions that need no docker: host path
# translation over mountinfo fixtures, the time budget, the build plan, the
# Dockerfile-stage check and the diff base. Run from anywhere:
#
#   .aidev/tests/integration_test.sh
set -uo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=.aidev/integration.sh
. "$here/integration.sh"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
failures=0

expect() {  # NAME EXPECTED ACTUAL
    if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$2], got [$3]"; failures=$((failures + 1)); fi
}

mountinfo() {  # NAME, lines on stdin
    cat > "$scratch/$1"; echo "$scratch/$1"
}

# --- host_path ---------------------------------------------------------------
host_root_only="$(mountinfo host-root-only <<'EOF'
25 1 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw
26 25 0:22 / /proc rw,nosuid shared:2 - proc proc rw
EOF
)"
host_split_home="$(mountinfo host-split-home <<'EOF'
25 1 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw
30 25 8:17 / /home rw,relatime shared:9 - ext4 /dev/sdb1 rw
31 25 0:45 / /run rw,nosuid shared:10 - tmpfs tmpfs rw
EOF
)"
# A worker container: its root an overlay, the checkout bound from /home/syncad/dan/hive.
worker_split_home="$(mountinfo worker-split-home <<'EOF'
800 700 0:120 / / rw,relatime - overlay overlay rw,lowerdir=/x
801 800 8:17 /syncad/dan/hive /home/worker/target rw,relatime - ext4 /dev/sdb1 rw
802 800 0:130 / /tmp rw,nosuid - tmpfs tmpfs rw
EOF
)"

expect "same namespace, one root filesystem" /srv/hive/tests \
    "$(host_path /srv/hive/tests "$host_root_only" "$host_root_only")"
expect "checkout on a separate /home partition, bound into a container" /home/syncad/dan/hive/test-results/x \
    "$(host_path /home/worker/target/test-results/x "$worker_split_home" "$host_split_home")"
expect "the bind mount's own point" /home/syncad/dan/hive \
    "$(host_path /home/worker/target "$worker_split_home" "$host_split_home")"
host_path /home/worker/target "$worker_split_home" "$host_root_only" > /dev/null
expect "the partition is not mounted on the daemon's host" 1 "$?"
host_path /tmp/work "$worker_split_home" "$host_split_home" > /dev/null
expect "a tmpfs only the container has" 1 "$?"

host_subdir_only="$(mountinfo host-subdir-only <<'EOF'
25 1 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw
30 25 8:17 /other /mnt/other rw,relatime - ext4 /dev/sdb1 rw
EOF
)"
host_path /home/worker/target "$worker_split_home" "$host_subdir_only" > /dev/null
expect "the daemon's host mounts only another subtree of the partition" 1 "$?"

host_nested="$(mountinfo host-nested <<'EOF'
25 1 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw
30 25 8:17 / /home rw,relatime - ext4 /dev/sdb1 rw
40 25 8:17 /syncad /srv/syncad rw,relatime - ext4 /dev/sdb1 rw
EOF
)"
expect "the most specific host mount of the partition" /srv/syncad/dan/hive/a \
    "$(host_path /home/worker/target/a "$worker_split_home" "$host_nested")"

worker_space="$(mountinfo worker-space <<'EOF'
800 700 0:120 / / rw,relatime - overlay overlay rw
801 800 8:17 /dan/my\040hive /work\040dir rw,relatime - ext4 /dev/sdb1 rw
EOF
)"
expect "escaped spaces" "/home/dan/my hive/x" \
    "$(host_path "/work dir/x" "$worker_space" "$host_split_home")"

# --- integration_deadline ----------------------------------------------------
HIVE_INTEGRATION_BUDGET=1200 HIVE_INTEGRATION_ACTIVITY_SECONDS=2400 HIVE_INTEGRATION_MARGIN=300
expect "no slot start: the step's own budget" $((10000 + 1200 - 300)) "$(integration_deadline 10000 10000 "")"
expect "the slot started long enough ago to leave less than the budget" $((10000 - 1500 + 2400 - 300)) \
    "$(integration_deadline 10000 10000 $((10000 - 1500)))"
expect "the slot started recently: the budget binds" $((10000 + 1200 - 300)) \
    "$(integration_deadline 10000 10000 $((10000 - 600)))"
expect "a slot start older than the activity is another run's" $((10000 + 1200 - 300)) \
    "$(integration_deadline 10000 10000 $((10000 - 5000)))"
expect "a slot start in the future is ignored" $((10000 + 1200 - 300)) \
    "$(integration_deadline 10000 10000 20000)"

# --- integration_plan --------------------------------------------------------
HIVE_INTEGRATION_WARM_BUILD_SECONDS=300 HIVE_INTEGRATION_COLD_BUILD_SECONDS=2700
integration_plan 900 1 warm 300 > /dev/null
expect "a warm build and a script fit" 0 "$?"
reason="$(integration_plan 900 2 cold 300)"
expect "no cache and too little time: skipped, naming sccache" sccache_unreachable "${reason%%:*}"
reason="$(integration_plan 700 2 warm 300)"
expect "warm, but too little time: out of time" out_of_time "${reason%%:*}"
integration_plan 6000 2 cold 300 > /dev/null
expect "no cache, but time for a cold build" 0 "$?"
redis_answers redis://127.0.0.1:1
expect "nothing listening is no cache" 1 "$?"

# --- instance_base_changed and diff_base, in a scratch repository -------------
repo="$scratch/repo"
git init -q "$repo" && cd "$repo" || exit 1
git config user.email t@t && git config user.name t
mkdir -p scripts .aidev
cp "$here/../Dockerfile" Dockerfile
printf 'branches:\n  integration: "aidev/integration"\n' > .aidev/project.yaml
echo setup > scripts/setup_ubuntu.sh && echo conf > scripts/openssl.conf
git add -A && git commit -qm base
base="$(git rev-parse HEAD)"
instance_base_changed "$base"
expect "nothing changed" 1 "$?"
sed -i 's/^ARG CI_IMAGE_TAG=.*/ARG CI_IMAGE_TAG=bumped/' Dockerfile
instance_base_changed "$base"
expect "CI_IMAGE_TAG bumped" 0 "$?"
git checkout -q Dockerfile
sed -i '/^FROM ubuntu:.* AS minimal-runtime$/a RUN true' Dockerfile
instance_base_changed "$base"
expect "a line added to minimal-runtime" 0 "$?"
git checkout -q Dockerfile
echo "# instance" >> Dockerfile
instance_base_changed "$base"
expect "a line added to the instance stage only" 1 "$?"
git checkout -q Dockerfile
echo more >> scripts/setup_ubuntu.sh
instance_base_changed "$base"
expect "a file the runtime stages ADD" 0 "$?"
git checkout -q scripts

expect "diff base from the gate" "AIDEV_BASE_REF=$base" "$(AIDEV_BASE_REF=$base AIDEV_IMPLEMENT_BASE_SHA= diff_base)"
expect "diff base from the workflow" "AIDEV_IMPLEMENT_BASE_SHA=$base" \
    "$(AIDEV_BASE_REF= AIDEV_IMPLEMENT_BASE_SHA=$base diff_base)"
AIDEV_BASE_REF=nonexistent AIDEV_IMPLEMENT_BASE_SHA= diff_base > /dev/null
expect "no base resolves" 1 "$?"

# --- integration-select.sh, run from the scratch repository -------------------
cp "$here/integration.sh" "$here/integration-select.sh" .aidev/
mkdir -p libraries/chain docker tests/integration
echo evaluator > libraries/chain/evaluator.cpp
git add -A && git commit -qm select-base
select_base="$(git rev-parse HEAD)"
run_select() {  # SELECT_ENV...: "EXIT|LAST LINE"
    local output rc
    output="$(env AIDEV_BASE_REF="$select_base" AIDEV_IMPLEMENT_BASE_SHA= HIVE_AIDEV_INTEGRATION=1 "$@" \
        .aidev/integration-select.sh 2>&1)"; rc=$?
    echo "$rc|${output##*$'\n'}"
}
result="$(run_select HIVE_AIDEV_INTEGRATION=0)"
expect "select: off" "77|off" "${result%%:*}"
result="$(run_select AIDEV_BASE_REF=nonexistent)"
expect "select: no diff base" "77|no diff base" "${result%%:*}"
echo changed >> libraries/chain/evaluator.cpp
result="$(run_select)"
expect "select: an evaluator change selects nothing" \
    "77|the diff from AIDEV_BASE_REF=$select_base selects no tests/integration script" "$result"
git checkout -q libraries
echo ci-only > tests/integration/test_ci_only.sh
result="$(run_select)"
expect "select: only a script needing CI's fixtures" \
    "77|the diff from AIDEV_BASE_REF=$select_base selects only scripts needing fixtures only CI has: test_ci_only.sh" \
    "$result"
echo image > docker/x.sh
result="$(run_select)"
expect "select: docker/ selects both scripts" \
    "0|selected: test_exchange_node_block_log_pruning.sh test_snapshot_bootstrap.sh" "$result"
expect "select: writes no test results" "" "$(git status --porcelain --ignored -- test-results)"

[ "$failures" -eq 0 ] && echo "all passed" || { echo "$failures failed"; exit 1; }

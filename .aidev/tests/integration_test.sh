#!/usr/bin/env bash
# Tests of .aidev/integration.sh's decisions that need no docker: host path
# translation over mountinfo fixtures, the time budget, the build plan, the
# Dockerfile-stage check and the diff base; and, where a docker daemon and the
# checks' image are at hand, that the image build's output in the checkout is
# this user's however its container ends. Run from anywhere:
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
integration_plan 900 1200 warm 300 testnet=500 > /dev/null
expect "a measured build and a script fit" 0 "$?"
expect "the plan names its inputs" \
    "plan: testnet image 500s, mainnet image unmeasured, the shortest script 300s, as measured on this host with the compiler cache warm: 800s needed, 900s left of the 1200s budget" \
    "$(integration_plan 900 1200 warm 300 testnet=500 mainnet= | head -1)"
reason="$(integration_plan 900 1200 cold 300 testnet=800 mainnet=700 | tail -1)"
expect "no cache and too little time for the cold builds measured here: skipped, naming sccache" \
    "sccache_unreachable: no compiler cache answers, and 900s left of the 1200s budget cannot fit 2 cold image build(s) and a script (1800s)" \
    "$reason"
reason="$(integration_plan 700 1000 warm 300 testnet=500 mainnet=200 | tail -1)"
expect "warm, but too little time: out of time, naming the budget and the need" \
    "out_of_time: 700s left of the 1000s budget (HIVE_INTEGRATION_BUDGET), 2 image build(s) and a script need 1000s" "$reason"
integration_plan 900 1200 cold 300 testnet= mainnet= > /dev/null
expect "builds not measured here are tried, however cold" 0 "$?"
reason="$(integration_plan 100 400 cold 120 testnet= | tail -1)"
expect "too little time for the script alone: out of time, not sccache" \
    "out_of_time: 100s left of the 400s budget (HIVE_INTEGRATION_BUDGET), 1 image build(s) and a script need 120s" "$reason"
integration_plan 900 1200 warm 300 testnet='>600' > /dev/null
expect "a build cut off here after less than the time left is tried again" 0 "$?"
integration_plan 900 1200 warm 300 testnet='>800' > /dev/null
expect "a build cut off here after more than the time left is not" 1 "$?"
redis_answers redis://127.0.0.1:1
expect "nothing listening is no cache" 1 "$?"
expect "nothing listening is a cold cache" cold "$(HIVE_SCCACHE_REDIS=redis://127.0.0.1:1 compiler_cache)"

# --- build_seconds, record_build_seconds and integration_preconditions --------
# Against a docker stub: `info` exits STUB_DOCKER_INFO, `volume ls` lists
# STUB_BUILD_VOLUME, and every other call is logged to STUB_LOG.
mkdir -p "$scratch/stub" "$scratch/dockercfg/cli-plugins"
cat > "$scratch/stub/docker" <<'STUB'
#!/usr/bin/env bash
case "$1 ${2:-}" in
    "info "*) exit "${STUB_DOCKER_INFO:-0}" ;;
    "volume ls") [ -z "${STUB_BUILD_VOLUME:-}" ] || echo "$STUB_BUILD_VOLUME" ;;
    *) echo "$*" >> "$STUB_LOG" ;;
esac
STUB
printf '#!/bin/sh\n' > "$scratch/dockercfg/cli-plugins/docker-compose"
chmod +x "$scratch/stub/docker" "$scratch/dockercfg/cli-plugins/docker-compose"
export STUB_LOG="$scratch/stub.log"
stub_path="$scratch/stub:$PATH"
measured() {  # VOLUME: what build_seconds reads from it
    STUB_BUILD_VOLUME="$1" PATH="$stub_path" build_seconds testnet warm
}
expect "a measured build" 412 "$(measured hive-integration-build-testnet-warm-412)"
expect "a build cut off" ">600" "$(measured hive-integration-build-testnet-warm-gt600)"
expect "none measured" "" "$(measured "")"
expect "a volume not naming seconds" "" "$(measured hive-integration-build-testnet-warm-x)"
recorded() {  # PREVIOUS_VOLUME RC SECONDS: the volume record_build_seconds creates
    : > "$STUB_LOG"
    STUB_BUILD_VOLUME="$1" PATH="$stub_path" step_skipped=77 record_build_seconds testnet warm "$2" "$3"
    sed -n 's/^volume create .* //p' "$STUB_LOG"
}
expect "a build recorded" hive-integration-build-testnet-warm-412 "$(recorded hive-integration-build-testnet-warm-900 0 412)"
expect "a cut-off build recorded as taking longer" hive-integration-build-testnet-warm-gt600 \
    "$(recorded hive-integration-build-testnet-warm-400 77 600)"
expect "a cut-off build shorter than one measured is not recorded" "" \
    "$(recorded hive-integration-build-testnet-warm-700 77 600)"
expect "a build skipped before it started is not recorded" "" "$(recorded "" 77 0)"
expect "a failed build is not recorded" "" "$(recorded "" 1 300)"
preconditions() {  # PRELUDE ENV...: "LAST LINE|EXIT" of integration_preconditions, 900 s left, cold
    local prelude="$1"; shift
    env PATH="$stub_path" DOCKER_CONFIG="$scratch/dockercfg" HIVE_INTEGRATION_BUDGET=1200 "$@" bash -c \
        '. "$1/integration.sh"; eval "$2"; out="$(integration_preconditions 900 cold test_hived_uid.sh)"; echo "${out##*$'"'\n'"'}|$?"' \
        _ "$here" "$prelude"
}
expect "preconditions: docker unreachable" "docker_capability_unavailable: no docker daemon reachable here|1" \
    "$(preconditions : STUB_DOCKER_INFO=1)"
expect "preconditions: no compose plugin" "docker_compose_unavailable: no docker compose plugin here|1" \
    "$(preconditions 'compose_plugin() { return 1; }')"
expect "preconditions: they hold, the plan last" \
    "plan: testnet image unmeasured, the shortest script 120s, as measured on this host with the compiler cache cold: 120s needed, 900s left of the 1200s budget|0" \
    "$(preconditions :)"
expect "preconditions: the time measured here" \
    "sccache_unreachable: no compiler cache answers, and 900s left of the 1200s budget cannot fit 1 cold image build(s) and a script (1120s)|1" \
    "$(preconditions : STUB_BUILD_VOLUME=hive-integration-build-testnet-cold-1000)"

# --- docker-shim/docker, ahead of a docker that prints its arguments ----------
mkdir -p "$scratch/cli"
printf '#!/usr/bin/env bash\nprintf "[%%s]" "$@"\n' > "$scratch/cli/docker"
chmod +x "$scratch/cli/docker"
shim() {  # OWNER ARGS...: what the docker behind the shim is called with
    local owner="$1"; shift
    env AIDEV_ENV_CONTAINER_OWNER="$owner" PATH="$here/docker-shim:$scratch/cli:$PATH" docker "$@"
}
expect "shim: run, owned" "[run][--label][aidev.env-container.owner=w 1][--oom-score-adj][900][--rm][img][a b]" \
    "$(shim "w 1" run --rm img "a b")"
expect "shim: create, owned" "[create][--label][aidev.env-container.owner=w][--oom-score-adj][900][img]" \
    "$(shim w create img)"
expect "shim: build, owned" "[build][--label][aidev.env-container.owner=w][-q][-t][t][.]" "$(shim w build -q -t t .)"
expect "shim: other commands, owned" "[ps][-aq][--filter][name=run]" "$(shim w ps -aq --filter name=run)"
expect "shim: run, no owner" "[run][--rm][img]" "$(shim "" run --rm img)"
expect "shim: build, no owner" "[build][-q][.]" "$(shim "" build -q .)"
expect "shim: no docker behind it" 127 "$(PATH="$here/docker-shim:/nonexistent" "$here/docker-shim/docker" ps 2> /dev/null; echo $?)"

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
# The host as the docker stub has it: a daemon, the compose plugin, no compiler
# cache, no build measured, and the suite's budget.
run_select() {  # SELECT_ENV...: "EXIT|LAST LINE"
    local output rc
    output="$(env AIDEV_BASE_REF="$select_base" AIDEV_IMPLEMENT_BASE_SHA= HIVE_AIDEV_INTEGRATION=1 \
        PATH="$stub_path" DOCKER_CONFIG="$scratch/dockercfg" HIVE_SCCACHE_REDIS=redis://127.0.0.1:1 \
        HIVE_INTEGRATION_BUDGET=1200 "$@" .aidev/integration-select.sh 2>&1)"; rc=$?
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
expect "select: docker/ selects the image's scripts" \
    "0|selected: test_exchange_node_block_log_pruning.sh test_hived_uid.sh test_snapshot_bootstrap.sh" "$result"
rm docker/x.sh
echo "# instance" >> Dockerfile
result="$(run_select)"
expect "select: Dockerfile selects the image's scripts" \
    "0|selected: test_exchange_node_block_log_pruning.sh test_hived_uid.sh test_snapshot_bootstrap.sh" "$result"
git checkout -q Dockerfile
echo more >> scripts/setup_ubuntu.sh
result="$(run_select)"
expect "select: setup_ubuntu.sh selects the image's scripts" \
    "0|selected: test_exchange_node_block_log_pruning.sh test_hived_uid.sh test_snapshot_bootstrap.sh" "$result"
git checkout -q scripts
echo uid >> tests/integration/test_hived_uid.sh
result="$(run_select STUB_DOCKER_INFO=1)"
expect "select: no docker daemon" \
    "77|skipped: docker_capability_unavailable: no docker daemon reachable here (selected: test_hived_uid.sh)" "$result"
result="$(run_select HIVE_INTEGRATION_BUDGET=400)"
expect "select: a budget too small for the script" \
    "77|skipped: out_of_time: 100s left of the 400s budget (HIVE_INTEGRATION_BUDGET), 1 image build(s) and a script need 120s (selected: test_hived_uid.sh)" \
    "$result"
result="$(run_select STUB_BUILD_VOLUME=hive-integration-build-testnet-cold-1000)"
expect "select: a cold build measured here longer than the budget" \
    "77|skipped: sccache_unreachable: no compiler cache answers, and 900s left of the 1200s budget cannot fit 1 cold image build(s) and a script (1120s) (selected: test_hived_uid.sh)" \
    "$result"
result="$(run_select)"
expect "select: test_hived_uid.sh, on a host that can run it" "0|selected: test_hived_uid.sh" "$result"
rm tests/integration/test_hived_uid.sh
expect "select: writes no test results" "" "$(git status --porcelain --ignored -- test-results)"

# --- the suite's verdict when it runs no script: run-checks.sh in the scratch repository
# run-checks.sh mirrors the checkout into /home/hived/source when it can; not this one.
if [ -w /home/hived ] && [ ! -e /home/hived/source ]; then
    echo "skip the suite's verdict: run-checks.sh would mirror this scratch checkout into /home/hived/source"
else
    cp "$here/run-checks.sh" "$here/build-jobs.sh" "$here/junit_cases.py" .aidev/
    echo uid >> tests/integration/test_hived_uid.sh
    run_suite() {  # ENV...: "EXIT|LAST LINE" of the suite as the `integration` suite runs it, its output in $scratch/suite.log
        local rc
        env AIDEV_BASE_REF="$select_base" AIDEV_IMPLEMENT_BASE_SHA= HIVE_AIDEV_INTEGRATION=1 \
            PATH="$stub_path" DOCKER_CONFIG="$scratch/dockercfg" HIVE_SCCACHE_REDIS=redis://127.0.0.1:1 "$@" \
            .aidev/run-checks.sh full-integration integration > "$scratch/suite.log" 2>&1; rc=$?
        echo "$rc|$(tail -1 "$scratch/suite.log")"
    }
    expect "suite: no docker daemon at run time: skipped, not passed" \
        "77|skipped: integration-test_ci_only: needs fixtures only CI has; run in GitLab CI; integration: docker_capability_unavailable: no docker daemon reachable here (selected: test_ci_only.sh test_hived_uid.sh)" \
        "$(run_suite STUB_DOCKER_INFO=1)"
    expect "suite: each skipped script's reason in its summary" 1 \
        "$(grep -cx '  integration-test_ci_only: needs fixtures only CI has; run in GitLab CI' "$scratch/suite.log")"
    expect "suite: the junit report has them skipped" 2 "$(grep -c '<skipped ' test-results/full-integration/junit.xml)"
    expect "suite: off: skipped" \
        "77|skipped: integration: off: HIVE_AIDEV_INTEGRATION is not 1 (.aidev/README.md, Integration scripts)" \
        "$(run_suite HIVE_AIDEV_INTEGRATION=0)"
    rm -rf tests/integration/test_hived_uid.sh test-results
fi

# --- hived_image's ownership hand-back, against a stub run-checks.sh -----------
# The scratch checkout lives in the real one's test-results/, which the daemon
# sees (host_path); a temporary directory may be this container's own.
runtime="$(grep -o 'registry[^"]*aidev-tests@sha256:[0-9a-f]*' "$here/project.yaml")"
if ! docker info > /dev/null 2>&1 || ! docker image inspect "$runtime" > /dev/null 2>&1; then
    echo "skip hived_image ownership: no docker daemon, or no $runtime here"
else
    mkdir -p "$here/../test-results"
    checkout="$(mktemp -d "$here/../test-results/integration-test-XXXXXX")"
    checkout_on_host=""
    trap 'rm -rf "$scratch"; rm -rf "$checkout" 2> /dev/null || { [ -n "$checkout_on_host" ] && docker run --rm --user 0 \
        -v "$checkout_on_host:/c" --entrypoint bash "$runtime" -c "rm -rf /c/* /c/.[!.]*"; rm -rf "$checkout"; }' EXIT
    cd "$checkout" && git init -q . || exit 1
    mkdir -p .aidev
    cat > .aidev/run-checks.sh <<'STUB'
#!/usr/bin/env bash
# What the build leaves, then the way the stub was told to end: .aidev/stub-end.
mkdir -p "test-results/$1/bin" && echo hived > "test-results/$1/bin/hived"
case "$(cat .aidev/stub-end)" in
    fail) echo "[609/609] failed" > "test-results/$1/$2.log"; exit 1 ;;
    grow) while :; do echo "[607/609]" >> "test-results/$1/$2.log"; sleep 0.2; done ;;
    oom) echo "[607/609]" > "test-results/$1/$2.log"; x="$(head -c 1000000000 /dev/zero | tr '\0' x)" ;;
esac
STUB
    chmod +x .aidev/run-checks.sh
    touch .gitlab-ci.yaml
    out="$PWD/test-results/full-integration" step_skipped=77 integration_prefix="hive-integration-test-$$"
    mkdir -p "$out"
    if ! read_host_mountinfo "$runtime" || ! host_sees "$PWD" .aidev/run-checks.sh "$runtime"; then
        echo "skip hived_image ownership: the checkout's path on the docker daemon's host does not resolve"
    else
        checkout_on_host="$(on_host "$PWD")"
        built="$PWD/test-results/full-integration-hived-testnet"
        not_mine() { find "$PWD/test-results" ! -uid "$(id -u)" | head -3 | tr '\n' ' '; }
        build_ending() {  # END DEADLINE_SECONDS: hived_image's exit, the build container ended by END
            echo "$1" > .aidev/stub-end
            rm -rf "$built"
            integration_deadline_at=$(( $(date +%s) + $2 ))
            hived_image testnet "$runtime" unused-tag 0 > "$scratch/hived_image.log" 2>&1
        }

        build_ending fail 600; rc=$?
        expect "a failed build: its exit" 1 "$rc"
        expect "a failed build: its output kept" "bin/hived hived-testnet.log" \
            "$(cd "$built" && find . -type f | sed 's|^\./||' | sort | tr '\n' ' ' | sed 's/ $//')"
        expect "a failed build: nothing left to another user" "" "$(not_mine)"

        build_ending grow 600 & pid=$!
        for _ in $(seq 100); do [ "$(wc -l 2> /dev/null < "$built/hived-testnet.log" || echo 0)" -ge 3 ] && break; sleep 0.5; done
        docker kill "$integration_prefix-build-testnet" > /dev/null
        wait "$pid"; rc=$?
        expect "a build killed mid-build: its exit" 137 "$rc"
        expect "a build killed mid-build: nothing left to another user" "" "$(not_mine)"

        build_ending grow 5; rc=$?
        expect "a build cut off by its timeout: skipped" 77 "$rc"
        expect "a build cut off by its timeout: nothing left to another user" "" "$(not_mine)"

        AIDEV_ENV_CONTAINER_MEMORY_BYTES=$(( 128 * 1024 * 1024 )) build_ending oom 600; rc=$?
        expect "a build OOM-killed at its cap: its exit" 137 "$rc"
        expect "a build OOM-killed at its cap: nothing left to another user" "" "$(not_mine)"
    fi
fi

[ "$failures" -eq 0 ] && echo "all passed" || { echo "$failures failed"; exit 1; }

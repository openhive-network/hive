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
#   - integration     the tests/integration scripts the diff selects (integration_table),
#                     each against a hived image built from the checkout, one junit case
#                     per script; needs a docker daemon (a `docker: true` suite)
#   - hived-testnet, hived-mainnet
#                     hived and the tools its image carries, into <report dir>/bin
#                     (what `integration` runs in the checks' image to build an image)
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
    ninja -j "${HIVE_BUILD_JOBS:-$(nproc)}" chain_test plugin_test assertion_id_verifier || rc=$?
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
    ninja -j "${HIVE_BUILD_JOBS:-$(nproc)}" hived cli_wallet shared_memory_file_util || return 1
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

# --- integration ------------------------------------------------------------
# The docker-based tests/integration scripts, run when the diff reaches them or
# what they exercise. A changed path starting with PATH selects SCRIPTS; with a
# LINES regex other than `.`, only when one of the path's changed lines or hunk
# headers matches it. A changed tests/integration/test_*.sh selects itself.
integration_table() {
    cat <<'TABLE'
# PATH                                           LINES                      SCRIPTS
docker/                                          .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh
Dockerfile                                       .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh
scripts/common.sh                                .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh
scripts/run_hived_img.sh                         .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh
scripts/hived_healthcheck.sh                     .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh
scripts/exchange_instance.conf                   .                          test_exchange_node_block_log_pruning.sh
scripts/build_and_setup_exchange_instance.sh     .                          test_exchange_node_block_log_pruning.sh
scripts/ci-helpers/docker_image_utils.sh         .                          test_exchange_node_block_log_pruning.sh
tests/integration/conftest.sh                    .                          test_exchange_node_block_log_pruning.sh
libraries/chainbase/                             .                          test_snapshot_bootstrap.sh
libraries/chain/database.cpp                     \<(resize|open)[a-z_]*\>   test_snapshot_bootstrap.sh
libraries/chain/include/hive/chain/database.hpp  \<(resize|open)[a-z_]*\>   test_snapshot_bootstrap.sh
TABLE
}

# What a script runs against and how long it may take: CI's
# snapshot_bootstrap_tests and exchange_node_block_log_pruning_test, each
# `<script> <hived image> <work dir>`. A script not named here needs fixtures
# only CI has.
integration_script_needs() {  # SCRIPT: NETWORK SECONDS
    case "$1" in
        test_snapshot_bootstrap.sh) echo testnet 2100 ;;
        test_exchange_node_block_log_pruning.sh) echo mainnet 1800 ;;
        *) return 1 ;;
    esac
}

# The commit the change is measured from: the gate's or the workflow's base, else
# where the checkout left the integration branch.
diff_base() {
    local branch b
    branch="$(sed -n 's/^  integration: *"\{0,1\}\([^"]*\)"\{0,1\} *$/\1/p' .aidev/project.yaml)"
    for b in "${AIDEV_BASE_REF:-}" "${AIDEV_IMPLEMENT_BASE_SHA:-}" "$(git merge-base HEAD "origin/${branch:-develop}" 2>/dev/null)"; do
        [ -n "$b" ] && git rev-parse -q --verify "$b^{commit}" > /dev/null && { echo "$b"; return 0; }
    done
    return 1
}

integration_selection() {  # BASE: the selected scripts, one per line
    local base="$1" changed path lines scripts f
    changed="$({ git diff --name-only "$base" --; git ls-files --others --exclude-standard; } | sort -u)"
    while read -r path lines scripts; do
        case "$path" in ""|"#"*) continue ;; esac
        while IFS= read -r f; do
            case "$f" in "$path"*) ;; *) continue ;; esac
            if [ "$lines" = . ] || { git diff -U0 "$base" -- "$f" | grep -E '^(@@|[-+])'
                    git ls-files --others --exclude-standard -- "$f" | xargs -r cat; } | grep -qE "$lines"; then
                printf '%s\n' $scripts
                break
            fi
        done <<< "$changed"
    done < <(integration_table)
    sed -n 's|^tests/integration/\(test_[^/]*\.sh\)$|\1|p' <<< "$changed"
}

skip_case() {  # NAME REASON
    echo "== $1: skipped: $2" >&2
    printf 'case\t%s\tskip\t0\t%s\n' "$1" "$2" >> "$cases"
}

# Where the docker daemon finds PATH: the source of the mount holding it, when
# that is a block device's filesystem the daemon's host has mounted at /. A path
# in a container's own filesystem (overlay, tmpfs) has none.
host_path() {  # PATH
    awk -v path="$(realpath -m "$1")" '
        { split($0, halves, " - "); split(halves[2], fs, " ")
          root = $4; point = $5; gsub(/\\040/, " ", root); gsub(/\\040/, " ", point)
          under = point == "/" ? "" : point
          if ((path == point || index(path, under "/") == 1) && length(point) >= length(best)) {
              best = point; best_under = under; best_root = root; best_source = fs[2] } }
        END { if (best_source !~ /^\/dev\//) exit 1
              print (best_root == "/" ? "" : best_root) substr(path, length(best_under) + 1) }' /proc/self/mountinfo
}

# The scripts' containers have fixed names (and the exchange one a fixed host
# port), so a host runs each script once at a time. The lock is a docker network,
# whose creation fails while one of that name exists; one older than any run is
# a killed run's, and is taken over. Each lock is labelled with the run holding it.
integration_lock() {  # SCRIPT
    local name="hive-integration-lock-${1%.sh}" created
    until docker network create --label "hive.integration.created=$(date +%s)" \
            --label "hive.integration.owner=$integration_prefix" "$name" > /dev/null 2>&1; do
        created="$(docker network inspect "$name" 2>/dev/null | python3 -c \
            'import json, sys; print(json.load(sys.stdin)[0]["Labels"].get("hive.integration.created", 0))' 2>/dev/null)"
        if [ -n "$created" ] && [ $(( $(date +%s) - created )) -gt 7200 ]; then
            echo "taking over $name, created at $created"
            docker network rm "$name" > /dev/null 2>&1
        else
            echo "waiting for $name: another run of $1 on this host"
            sleep 15
        fi
    done
}

compose_plugin() {
    local d
    for d in "${DOCKER_CONFIG:-$HOME/.docker}/cli-plugins" /usr/local/lib/docker/cli-plugins \
        /usr/local/libexec/docker/cli-plugins /usr/lib/docker/cli-plugins /usr/libexec/docker/cli-plugins; do
        [ -x "$d/docker-compose" ] && { echo "$d/docker-compose"; return 0; }
    done
    return 1
}

# The image the scripts run in: the checks' image (bash, the docker CLI, wget,
# curl) with this host's compose plugin, which test_snapshot_bootstrap.sh uses.
integration_runner_image() {  # RUNTIME_IMAGE TAG
    local stage="$out/runner"
    mkdir -p "$stage" && cp "$(compose_plugin)" "$stage/docker-compose" || return 1
    printf 'FROM %s\nCOPY docker-compose /usr/libexec/docker/cli-plugins/docker-compose\n' "$1" > "$stage/Dockerfile"
    docker build -q -t "$2" "$stage"
}

# NETWORK's binaries built in the checks' own image, with CI's paths and
# arguments (so CI's compiler cache) and the checkout and its git directory at
# the paths they have here; then the Dockerfile's instance stage, its build stage
# replaced by those binaries and the checkout's scripts, as CI's image has them.
# The build runs as root: link_gitdirs may have to create a submodule's git path
# outside /home/hived, and so trust a checkout it does not own. Its report is
# handed back to this user.
hived_image() {  # NETWORK RUNTIME_IMAGE TAG
    local net="$1" built="$out-hived-$1" stage="$out/image-$1" git_dir
    git_dir="$(realpath "$(git rev-parse --git-common-dir)")"
    docker run --rm --name "$integration_prefix-build-$net" --network host --user 0 \
        -v "$(host_path "$git_dir"):$git_dir" -v "$(host_path "$PWD"):$PWD" -w "$PWD" \
        -e SCCACHE_REDIS="$(sed -n 's/^ *SCCACHE_REDIS: *"\(.*\)"$/\1/p' .gitlab-ci.yaml | head -1)" \
        -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0='*' \
        -e HIVE_BUILD_JOBS "$2" bash -c '.aidev/run-checks.sh "$1" "$2"; rc=$?; chown -R "$3" "test-results/$1"; exit "$rc"' \
        _ "${built##*/}" "hived-$net" "$(id -u):$(id -g)" || return 1
    rm -rf "$stage"
    mkdir -p "$stage/build/home/hived/bin" "$stage/build/home/hived/source/doc" "$stage/context/docker" || return 1
    cp "$built/bin/"* "$stage/build/home/hived/bin/" && cp doc/example_config.ini "$stage/build/home/hived/source/doc/" \
        && cp -r scripts "$stage/build/home/hived/source/" && cp docker/docker_entrypoint.sh "$stage/context/docker/" || return 1
    docker build --target=instance --build-context "build=$stage/build" \
        -f Dockerfile -t "$3" "$stage/context"
    local rc=$?
    rm -rf "$stage" "$built/bin"
    return "$rc"
}

# Runs as root, because hived's containers leave the work directory to their own
# user; afterwards only the logs stay, owned by this user. The lock is released by
# integration_cleanup, once the containers the script left are gone.
integration_script() {  # SCRIPT IMAGE SECONDS RUNNER_IMAGE
    local dir="$out/${1%.sh}" host rc=0
    mkdir -p "$dir" && host="$(host_path "$dir")" || return 1
    integration_lock "$1"
    docker run --rm --name "$integration_prefix-${1%.sh}" --network host --user 0 \
        -v /var/run/docker.sock:/var/run/docker.sock -v "$(host_path "$PWD"):$(host_path "$PWD")" -w "$host" \
        "$4" timeout -k 30 "$3" "$(host_path "$PWD")/tests/integration/$1" "$2" "$host/work" || rc=$?
    docker run --rm --user 0 -v "$host:$host" --entrypoint bash "$4" -c \
        'find "$1" -type f ! -name "*.log" -delete; find "$1" -mindepth 1 -depth -type d -empty -delete; chown -R "$2" "$1"' \
        _ "$host" "$(id -u):$(id -g)" || true
    return "$rc"
}

# The run's containers, those the scripts left under their fixed names among them
# (each runs one of this run's images), then its locks, then its images: by ID, so
# the tags a script gave them go too.
integration_cleanup() {
    local image ids=()
    docker ps -aq --filter "name=^$integration_prefix-" | xargs -r docker rm -f > /dev/null 2>&1
    for image in "${integration_images[@]}"; do
        docker ps -aq --filter "ancestor=$image" | xargs -r docker rm -f > /dev/null 2>&1
        ids+=("$(docker image inspect -f '{{.Id}}' "$image" 2>/dev/null)")
    done
    docker network ls -q --filter "label=hive.integration.owner=$integration_prefix" | xargs -r docker network rm > /dev/null 2>&1
    docker image rm -f "${ids[@]}" > /dev/null 2>&1 || true
}

integration() {
    local base selected s needs net runtime runner scripts=()
    if ! base="$(diff_base)"; then
        skip_case integration "no diff base: none of AIDEV_BASE_REF, AIDEV_IMPLEMENT_BASE_SHA or a merge base with the integration branch resolves"
        return 0
    fi
    selected="$(integration_selection "$base" | sort -u)"
    if [ -z "$selected" ]; then
        skip_case integration "the diff from ${base:0:12} selects no tests/integration script"
        return 0
    fi
    declare -A images=()
    for s in $selected; do
        if needs="$(integration_script_needs "$s")"; then
            scripts+=("$s"); images[${needs% *}]=1
        else
            skip_case "integration-${s%.sh}" "needs fixtures only CI has; run in GitLab CI"
        fi
    done
    [ "${#scripts[@]}" -gt 0 ] || return 0
    if ! docker info > /dev/null 2>&1; then
        skip_case integration "docker_capability_unavailable: no docker daemon reachable here (selected: ${selected//$'\n'/ })"
        return 0
    fi
    if ! host_path "$PWD" > /dev/null || ! host_path "$(git rev-parse --git-common-dir)" > /dev/null; then
        skip_case integration "docker_paths_unshared: the checkout is not on a filesystem the docker daemon's host mounts (selected: ${selected//$'\n'/ })"
        return 0
    fi
    if ! compose_plugin > /dev/null; then
        skip_case integration "docker_compose_unavailable: no docker compose plugin here (selected: ${selected//$'\n'/ })"
        return 0
    fi
    echo "== integration: ${scripts[*]} (diff from ${base:0:12})" >&2
    integration_prefix="hive-integration-$(od -An -N4 -tx1 /dev/urandom | tr -d ' \n')"
    runtime="$(grep -o 'registry[^"]*aidev-tests@sha256:[0-9a-f]*' .aidev/project.yaml)"
    runner="$integration_prefix-runner:latest"
    integration_images=("$runner")
    # buildx keeps its state in ~/.docker, which a sandboxed worker may not write.
    export BUILDX_CONFIG="$out/.buildx"
    trap 'integration_cleanup; exit 143' TERM
    trap integration_cleanup EXIT
    docker image inspect "$runtime" > /dev/null 2>&1 || docker pull -q "$runtime" > /dev/null || return 1
    step integration-runner integration_runner_image "$runtime" "$runner" || return 1
    # One at a time: parts of the vendored libraries' build write into the source tree.
    for net in "${!images[@]}"; do
        images[$net]="registry.gitlab.syncad.com/hive/hive$([ "$net" = testnet ] && echo /testnet):$integration_prefix"
        integration_images+=("${images[$net]}")
        step "integration-image-$net" hived_image "$net" "$runtime" "${images[$net]}" || return 1
    done
    for s in "${scripts[@]}"; do
        needs="$(integration_script_needs "$s")"
        step "integration-${s%.sh}" integration_script "$s" "${images[${needs% *}]}" "${needs#* }" "$runner" &
    done
    wait_all
}

wait_all() {  # whether every background job of this shell succeeded
    local pid rc=0
    for pid in $(jobs -p); do wait "$pid" || rc=1; done
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

# Polls rather than `wait -n`: bash 4.4's `wait -n` waits only for a job that
# ends after it is called, so a step that ended just before would keep its share.
# Each step runs in a session of its own, so a fresh bash given this script's
# functions and settings; a background job of a script leads no process group,
# so setsid makes the job itself the session leader, without forking.
concurrently() {  # STEP...: sharing $test_jobs, a step's share handed on when it ends
    local s pid rc=0 running ended
    step_of=()
    slots="$(mktemp -d)" || return 1
    share_slots "$slots" "$@"
    for s in "$@"; do
        setsid "$BASH" -c "set -uo pipefail; $(declare -p out cases timings src bld); $(declare -f)"'
            test_slots="$1"; run "$2"' step "$slots/$s" "$s" < /dev/null &
        step_of[$!]="$s"
    done
    while [ "${#step_of[@]}" -gt 0 ]; do
        sleep 1
        running=" $(jobs -pr | tr '\n' ' ') " ended=0
        for pid in "${!step_of[@]}"; do
            [[ "$running" == *" $pid "* ]] && continue
            wait "$pid" || rc=1
            unset "step_of[$pid]"; ended=1
        done
        [ "$ended" -eq 1 ] && [ "${#step_of[@]}" -gt 0 ] || continue
        share_slots "$slots" "${step_of[@]}"
        for s in "${step_of[@]}"; do echo "== $s: now $(cat "$slots/$s") test processes" >&2; done
    done
    rm -rf "$slots"; slots=""
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

for group in "$@"; do
    IFS=, read -ra group_steps <<< "$group"
    concurrently "${group_steps[@]}" || status=1
done
printf 'total\t%s\t%s\n' "$SECONDS" "$status" >> "$timings"
echo "== wall time per step (seconds):" >&2
cat "$timings" >&2
python3 .aidev/junit_cases.py "$out/junit.xml" "$suite" "$cases"
exit "$status"

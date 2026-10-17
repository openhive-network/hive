# shellcheck shell=bash
# The `integration` step of run-checks.sh, which sources this file: the docker-based
# tests/integration scripts the diff selects, each against a hived image built from
# the checkout. It runs only under HIVE_AIDEV_INTEGRATION=1 (.aidev/README.md).
# The functions that use run-checks.sh's globals ($out, $cases, step) are called
# only from it; the rest are tested by .aidev/tests/integration_test.sh.

# Time the step may take: its own budget from when it starts, and the activity's
# start-to-close (aidev's run_full_tests) from when the slot's first suite started,
# less a margin for what aidev does before and after the suites. aidev hands a suite
# no deadline, so the step keeps its own. Exported: each step runs in a fresh bash.
export HIVE_INTEGRATION_BUDGET="${HIVE_INTEGRATION_BUDGET:-1200}"
export HIVE_INTEGRATION_ACTIVITY_SECONDS="${HIVE_INTEGRATION_ACTIVITY_SECONDS:-2400}"
export HIVE_INTEGRATION_MARGIN="${HIVE_INTEGRATION_MARGIN:-300}"
# What one image's build is planned at: on CI's compiler cache, and cold (30-60 min).
export HIVE_INTEGRATION_WARM_BUILD_SECONDS="${HIVE_INTEGRATION_WARM_BUILD_SECONDS:-300}"
export HIVE_INTEGRATION_COLD_BUILD_SECONDS="${HIVE_INTEGRATION_COLD_BUILD_SECONDS:-2700}"

# A changed path starting with PATH selects SCRIPTS; with a LINES regex other than
# `.`, only when one of the path's changed lines or hunk headers matches it. A
# changed tests/integration/test_*.sh selects itself.
integration_table() {
    cat <<'TABLE'
# PATH                                           LINES                      SCRIPTS
docker/                                          .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh test_hived_uid.sh
Dockerfile                                       .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh test_hived_uid.sh
scripts/common.sh                                .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh
scripts/run_hived_img.sh                         .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh
scripts/hived_healthcheck.sh                     .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh
scripts/setup_ubuntu.sh                          .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh test_hived_uid.sh
scripts/openssl.conf                             .                          test_snapshot_bootstrap.sh test_exchange_node_block_log_pruning.sh
scripts/exchange_instance.conf                   .                          test_exchange_node_block_log_pruning.sh
scripts/build_and_setup_exchange_instance.sh     .                          test_exchange_node_block_log_pruning.sh
scripts/ci-helpers/docker_image_utils.sh         .                          test_exchange_node_block_log_pruning.sh
tests/integration/conftest.sh                    .                          test_exchange_node_block_log_pruning.sh
libraries/chainbase/                             .                          test_snapshot_bootstrap.sh
libraries/chain/database.cpp                     \<(resize|open)[a-z_]*\>   test_snapshot_bootstrap.sh
libraries/chain/include/hive/chain/database.hpp  \<(resize|open)[a-z_]*\>   test_snapshot_bootstrap.sh
TABLE
}

# What a script runs against, how long CI gives it, and the least time worth
# starting it with: CI's snapshot_bootstrap_tests, hived_uid_test and
# exchange_node_block_log_pruning_test, each `<script> <hived image> <work dir>`.
# A script not named here needs fixtures only CI has.
integration_script_needs() {  # SCRIPT: NETWORK SECONDS MIN_SECONDS
    case "$1" in
        test_snapshot_bootstrap.sh) echo testnet 2100 300 ;;
        test_exchange_node_block_log_pruning.sh) echo mainnet 1800 600 ;;
        test_hived_uid.sh) echo testnet 600 120 ;;
        *) return 1 ;;
    esac
}

# The commit the change is measured from, as VARIABLE=SHA: the gate's base, the
# workflow's, else where the checkout left the integration branch.
diff_base() {
    local branch name b
    branch="$(sed -n 's/^  integration: *"\{0,1\}\([^"]*\)"\{0,1\} *$/\1/p' .aidev/project.yaml)"
    for name in AIDEV_BASE_REF AIDEV_IMPLEMENT_BASE_SHA; do
        b="${!name:-}"
        [ -n "$b" ] && b="$(git rev-parse -q --verify "$b^{commit}")" && { echo "$name=$b"; return 0; }
    done
    b="$(git merge-base HEAD "origin/${branch:-develop}" 2>/dev/null)" && [ -n "$b" ] \
        && { echo "merge-base(origin/${branch:-develop})=$b"; return 0; }
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

# Whether the diff from BASE changes what the Dockerfile's runtime stages build: a
# line above its `build` stage (CI_IMAGE_TAG among them) or a file they ADD. The
# registry's minimal-runtime:${CI_IMAGE_TAG} is then not what the change builds.
instance_base_changed() {  # BASE
    local build_line
    [ -n "$(git diff --name-only "$1" -- scripts/setup_ubuntu.sh scripts/openssl.conf)" ] && return 0
    build_line="$(grep -n '^FROM .* AS build$' Dockerfile | head -1 | cut -d: -f1)"
    [ -n "$build_line" ] || return 0
    git diff -U0 "$1" -- Dockerfile | awk -v limit="$build_line" '
        /^@@/ { split($3, new, ","); if (substr(new[1], 2) + 0 < limit) found = 1 }
        END { exit !found }'
}

# Where the docker daemon finds PATH, from this process's mount table and the
# daemon host's: the mount with the longest mount point holding PATH gives its
# filesystem (major:minor) and the path within it; the host mount of that
# filesystem whose root holds that path most closely gives the host path. No
# such mount on either side fails; so does a path in a filesystem only this
# mount namespace has (a container's tmpfs).
host_path() {  # PATH LOCAL_MOUNTINFO HOST_MOUNTINFO
    awk -v path="$(realpath -m "$1")" '
        function unescape(s,   out, i) {
            out = ""
            for (i = 1; i <= length(s); i++)
                if (substr(s, i, 1) == "\\" && substr(s, i + 1, 3) ~ /^[0-7][0-7][0-7]$/) {
                    out = out sprintf("%c", (substr(s, i + 1, 1) * 64) + (substr(s, i + 2, 1) * 8) + substr(s, i + 3, 1)); i += 3
                } else out = out substr(s, i, 1)
            return out
        }
        function under(p, dir) { return dir == "/" || p == dir || index(p, dir "/") == 1 }
        function rest(p, dir) { return dir == "/" ? p : substr(p, length(dir) + 1) }
        FNR == 1 { file++ }
        file == 1 {
            point = unescape($5)
            if (under(path, point) && length(point) >= length(best_point)) {
                best_point = point; dev = $3; inner = unescape($4) rest(path, point)
                if (inner != "/") sub(/^\/\//, "/", inner)
            }
            next
        }
        file == 2 && $3 == dev {
            root = unescape($4)
            if (under(inner, root) && (host_root == "" || length(root) > length(host_root))) {
                host_root = root; point = unescape($5)
                host = (point == "/" ? "" : point) rest(inner, root)
                if (host == "") host = "/"
            }
        }
        END { if (dev == "" || host_root == "") exit 1; print host }' "$2" "$3"
}

# The epoch second the step has to end by (see HIVE_INTEGRATION_* above). A slot
# start in the future, or one older than the activity, is another run's: ignored.
integration_deadline() {  # NOW STEP_STARTED SLOT_STARTED|""
    local by=$(( $2 + HIVE_INTEGRATION_BUDGET )) slot="${3:-}"
    if [ -n "$slot" ] && [ "$slot" -le "$1" ] && [ $(( $1 - slot )) -lt "$HIVE_INTEGRATION_ACTIVITY_SECONDS" ] \
            && [ $(( slot + HIVE_INTEGRATION_ACTIVITY_SECONDS )) -lt "$by" ]; then
        by=$(( slot + HIVE_INTEGRATION_ACTIVITY_SECONDS ))
    fi
    echo $(( by - HIVE_INTEGRATION_MARGIN ))
}

# Whether IMAGES image builds and then the shortest script fit in REMAINING
# seconds, the builds planned warm or cold by whether CI's compiler cache answers;
# when they don't, prints the skip reason.
integration_plan() {  # REMAINING IMAGES CACHE(warm|cold) SCRIPT_MIN_SECONDS
    local per need
    per=$([ "$3" = warm ] && echo "$HIVE_INTEGRATION_WARM_BUILD_SECONDS" || echo "$HIVE_INTEGRATION_COLD_BUILD_SECONDS")
    need=$(( $2 * per + $4 ))
    [ "$1" -ge "$need" ] && return 0
    if [ "$3" = warm ]; then
        echo "out_of_time: ${1}s left, $2 image build(s) and a script need ${need}s"
    else
        echo "sccache_unreachable: no compiler cache answers, and ${1}s left cannot fit $2 cold image build(s) and a script (${need}s)"
    fi
    return 1
}

redis_answers() {  # redis://HOST:PORT: whether it answers PING within 3 s
    local hp="${1#redis://}"; hp="${hp%%/*}"
    timeout 3 bash -c 'exec 3<>"/dev/tcp/$1/$2" && printf "PING\r\n" >&3 && head -c5 <&3 | grep -q PONG' \
        _ "${hp%:*}" "${hp##*:}" 2>/dev/null
}

# CI's own Redis (HIVE_SCCACHE_REDIS overrides it), which the stack's relay
# forwards to; a host-side suite reaches it directly or not at all.
ci_sccache_redis() {
    [ -n "${HIVE_SCCACHE_REDIS:-}" ] && { echo "$HIVE_SCCACHE_REDIS"; return; }
    sed -n 's/^ *SCCACHE_REDIS: *"\(.*\)"$/\1/p' .gitlab-ci.yaml | head -1
}

# --- the step: these use run-checks.sh's $out, $cases and step ----------------

skip_case() {  # NAME REASON
    echo "== $1: skipped: $2" >&2
    printf 'case\t%s\tskip\t0\t%s\n' "$1" "$2" >> "$cases"
}

remaining() { echo $(( integration_deadline_at - $(date +%s) )); }

# Whether RC is `timeout -k`'s: its TERM, or its KILL once the deadline passed
# (a KILL before it is something else's, an OOM kill say).
timed_out() {  # RC
    [ "$1" -eq 124 ] || { [ "$1" -eq 137 ] && [ "$(remaining)" -le 0 ]; }
}

# A step function's out-of-time ending: step records the case skipped, with the
# log's last line as the reason.
out_of_time() {  # WHAT
    echo "out_of_time: $1 did not finish by the integration deadline"
    return "$step_skipped"
}

# The daemon's mount table (else its host's init's), read by a container in the
# host's PID namespace.
read_host_mountinfo() {  # RUNTIME_IMAGE
    docker run --rm --pid host --entrypoint bash "$1" -c '
        for p in /proc/[0-9]*; do [ "$(cat "$p/comm" 2>/dev/null)" = dockerd ] && exec cat "$p/mountinfo"; done
        exec cat /proc/1/mountinfo' > "$out/host-mountinfo" 2>/dev/null && [ -s "$out/host-mountinfo" ]
}

on_host() {  # PATH
    host_path "$1" /proc/self/mountinfo "$out/host-mountinfo"
}

# Whether the daemon, given the host path translated for DIR, sees DIR: FILE
# under it. Any doubt in the translation shows here rather than as a failed build.
host_sees() {  # DIR FILE RUNTIME_IMAGE
    local hp; hp="$(on_host "$1")" || return 1
    docker run --rm -v "$hp:/probe:ro" --entrypoint test "$3" -f "/probe/$2" > /dev/null 2>&1
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
        elif [ "$(remaining)" -le 0 ]; then
            return 1
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
# curl) with this host's compose plugin, which test_snapshot_bootstrap.sh uses,
# and the step's docker shim ahead of the docker CLI.
integration_runner_image() {  # RUNTIME_IMAGE TAG
    local stage="$out/runner"
    mkdir -p "$stage" && cp "$(compose_plugin)" "$stage/docker-compose" && cp .aidev/docker-shim/docker "$stage/" || return 1
    printf 'FROM %s\nCOPY docker-compose /usr/libexec/docker/cli-plugins/docker-compose\nCOPY docker /opt/aidev/bin/docker\nENV PATH=/opt/aidev/bin:$PATH\n' \
        "$1" > "$stage/Dockerfile"
    docker build -q -t "$2" "$stage"
}

# NETWORK's binaries built in the checks' own image, with CI's paths and
# arguments and the checkout and its git directory at the paths they have here;
# sccache there probes CI's Redis (HIVE_SCCACHE_REDIS) and else builds cold. Then
# the Dockerfile's instance stage, its build stage replaced by those binaries and
# the checkout's scripts, as CI's image has them, on the registry's minimal-runtime
# or, when the change touches the runtime stages (INLINE_BASE=1), on that stage
# built from the checkout. The build runs as root: link_gitdirs may have to create
# a submodule's git path outside /home/hived, and so trust a checkout it does not
# own. Its report is handed back to this user. It is capped at the suite's own
# container's memory (AIDEV_ENV_CONTAINER_MEMORY_BYTES), when AIDEV gives one, with
# no swap beyond it, as AIDEV caps its own containers.
hived_image() {  # NETWORK RUNTIME_IMAGE TAG INLINE_BASE
    local net="$1" built="$out-hived-$1" stage="$out/image-$1" git_dir rc=0 base_arg=() cap=()
    git_dir="$(realpath "$(git rev-parse --git-common-dir)")"
    [ -n "${AIDEV_ENV_CONTAINER_MEMORY_BYTES:-}" ] && cap=(--memory "$AIDEV_ENV_CONTAINER_MEMORY_BYTES" --memory-swap "$AIDEV_ENV_CONTAINER_MEMORY_BYTES")
    [ "$(remaining)" -gt 0 ] || { out_of_time "the $net binaries' build"; return; }
    docker run --rm --name "$integration_prefix-build-$net" --network host --user 0 "${cap[@]}" \
        -v "$(on_host "$git_dir"):$git_dir" -v "$(on_host "$PWD"):$PWD" -w "$PWD" \
        -e HIVE_SCCACHE_REDIS="$(ci_sccache_redis)" \
        -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0='*' \
        -e HIVE_BUILD_JOBS -e HIVE_BUILD_JOB_MEMORY_MB "$2" timeout -k 30 "$(remaining)" \
        bash -c '.aidev/run-checks.sh "$1" "$2"; rc=$?; chown -R "$3" "test-results/$1"; exit "$rc"' \
        _ "${built##*/}" "hived-$net" "$(id -u):$(id -g)" || rc=$?
    timed_out "$rc" && { out_of_time "the $net binaries' build"; return; }
    [ "$rc" -eq 0 ] || return "$rc"
    rm -rf "$stage"
    mkdir -p "$stage/build/home/hived/bin" "$stage/build/home/hived/source/doc" "$stage/context/docker" \
        "$stage/context/scripts" || return 1
    cp "$built/bin/"* "$stage/build/home/hived/bin/" && cp doc/example_config.ini "$stage/build/home/hived/source/doc/" \
        && cp -r scripts "$stage/build/home/hived/source/" && cp docker/docker_entrypoint.sh "$stage/context/docker/" \
        && cp scripts/setup_ubuntu.sh scripts/openssl.conf "$stage/context/scripts/" || return 1
    [ "$4" = 1 ] && base_arg=(--build-arg INSTANCE_BASE_IMAGE=minimal-runtime)
    [ "$(remaining)" -gt 0 ] || { rm -rf "$stage" "$built/bin"; out_of_time "the $net image's build"; return; }
    timeout -k 30 "$(remaining)" docker build --target=instance --build-context "build=$stage/build" \
        "${base_arg[@]}" -f Dockerfile -t "$3" "$stage/context" || rc=$?
    rm -rf "$stage" "$built/bin"
    timed_out "$rc" && { out_of_time "the $net image's build"; return; }
    return "$rc"
}

# Runs as root, because hived's containers leave the work directory to their own
# user; afterwards only the logs stay, owned by this user. The lock is released by
# integration_cleanup, once the containers the script left are gone. The script
# gets CI's timeout or what is left before the deadline, whichever is shorter: a
# script stopped by CI's is a failure, one stopped by the deadline is skipped. The
# script is told when it is stopped (HIVE_INTEGRATION_DEADLINE, an epoch second),
# never later than the deadline, so its steps end before it is.
integration_script() {  # SCRIPT IMAGE SECONDS MIN_SECONDS RUNNER_IMAGE
    local dir="$out/${1%.sh}" host checkout rc=0 seconds="$3" left
    mkdir -p "$dir" && host="$(on_host "$dir")" && checkout="$(on_host "$PWD")" || return 1
    integration_lock "$1" || { out_of_time "waiting for another run of $1 on this host"; return; }
    left="$(remaining)"
    if [ "$left" -lt "$4" ]; then
        echo "out_of_time: ${left}s left before the integration deadline, $1 needs at least $4s"
        return "$step_skipped"
    fi
    [ "$left" -lt "$seconds" ] && seconds="$left"
    echo "$1: ${seconds}s (CI gives it $3s)"
    docker run --rm --name "$integration_prefix-${1%.sh}" --network host --user 0 \
        -v /var/run/docker.sock:/var/run/docker.sock -v "$checkout:$checkout" -w "$host" \
        -e AIDEV_ENV_CONTAINER_OWNER -e HIVE_INTEGRATION_DEADLINE="$(( $(date +%s) + seconds ))" "$5" timeout -k 30 "$seconds" "$checkout/tests/integration/$1" "$2" "$host/work" || rc=$?
    docker run --rm --user 0 -v "$host:$host" --entrypoint bash "$5" -c \
        'find "$1" -type f ! -name "*.log" -delete; find "$1" -mindepth 1 -depth -type d -empty -delete; chown -R "$2" "$1"' \
        _ "$host" "$(id -u):$(id -g)" || true
    if timed_out "$rc"; then
        [ "$seconds" -lt "$3" ] && { out_of_time "$1"; return; }
        echo "$1 timed out at CI's ${3}s"
    fi
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
    local started base base_sha selected s needs net runtime runner cache inline=0 slot_started pid rc
    local scripts=() pids=() min_script=0 failed=0
    if [ "${HIVE_AIDEV_INTEGRATION:-0}" != 1 ]; then
        skip_case integration "off: HIVE_AIDEV_INTEGRATION is not 1 (.aidev/README.md, Integration scripts)"
        return 0
    fi
    started="$(date +%s)"
    slot_started="$(cat "${out%-integration}/started-at" 2>/dev/null)"
    [ "$slot_started" -eq "$slot_started" ] 2>/dev/null || slot_started=""
    integration_deadline_at="$(integration_deadline "$started" "$started" "$slot_started")"
    echo "== integration: deadline in $(remaining)s (budget ${HIVE_INTEGRATION_BUDGET}s, slot started $([ -n "$slot_started" ] && echo "$(( started - slot_started ))s ago" || echo here))" >&2
    if ! base="$(diff_base)"; then
        skip_case integration "no diff base: none of AIDEV_BASE_REF, AIDEV_IMPLEMENT_BASE_SHA or a merge base with the integration branch resolves"
        return 0
    fi
    base_sha="${base#*=}"
    echo "== integration: diff base: $base" >&2
    selected="$(integration_selection "$base_sha" | sort -u)"
    if [ -z "$selected" ]; then
        skip_case integration "the diff from $base selects no tests/integration script"
        return 0
    fi
    declare -A images=()
    for s in $selected; do
        if needs="$(integration_script_needs "$s")"; then
            scripts+=("$s"); read -r net _ needs <<< "$needs"; images[$net]=1
            [ "$min_script" -ne 0 ] && [ "$min_script" -le "$needs" ] || min_script="$needs"
        else
            skip_case "integration-${s%.sh}" "needs fixtures only CI has; run in GitLab CI"
        fi
    done
    [ "${#scripts[@]}" -gt 0 ] || return 0
    if ! docker info > /dev/null 2>&1; then
        skip_case integration "docker_capability_unavailable: no docker daemon reachable here (selected: ${selected//$'\n'/ })"
        return 0
    fi
    if ! compose_plugin > /dev/null; then
        skip_case integration "docker_compose_unavailable: no docker compose plugin here (selected: ${selected//$'\n'/ })"
        return 0
    fi
    cache=cold
    redis_answers "$(ci_sccache_redis)" && cache=warm
    echo "== integration: compiler cache $cache ($(ci_sccache_redis))" >&2
    if ! s="$(integration_plan "$(remaining)" "${#images[@]}" "$cache" "$min_script")"; then
        skip_case integration "$s (selected: ${selected//$'\n'/ })"
        return 0
    fi
    # Every container the step starts on the host's daemon, which AIDEV does not
    # see as the suite's, goes through the shim (and so do the scripts', in the runner).
    PATH="$PWD/.aidev/docker-shim:$PATH"
    runtime="$(grep -o 'registry[^"]*aidev-tests@sha256:[0-9a-f]*' .aidev/project.yaml)"
    docker image inspect "$runtime" > /dev/null 2>&1 || docker pull -q "$runtime" > /dev/null || return 1
    if ! read_host_mountinfo "$runtime" || ! host_sees "$PWD" .aidev/run-checks.sh "$runtime" \
            || ! host_sees "$(git rev-parse --git-common-dir)" HEAD "$runtime"; then
        skip_case integration "docker_paths_unshared: the checkout's path on the docker daemon's host cannot be resolved from the mount tables (selected: ${selected//$'\n'/ })"
        return 0
    fi
    if instance_base_changed "$base_sha"; then
        inline=1
        echo "== integration: image base: the Dockerfile's minimal-runtime stage, built from the change" >&2
    else
        s="$(sed -n 's/^ARG CI_REGISTRY_IMAGE=//p' Dockerfile)minimal-runtime:$(sed -n 's/^ARG CI_IMAGE_TAG=//p' Dockerfile)"
        if ! docker image inspect "$s" > /dev/null 2>&1 && ! docker pull -q "$s" > /dev/null 2>&1; then
            skip_case integration "instance_base_unpublished: $s cannot be pulled (selected: ${selected//$'\n'/ })"
            return 0
        fi
        echo "== integration: image base: $s" >&2
    fi
    echo "== integration: ${scripts[*]}" >&2
    integration_prefix="hive-integration-$(od -An -N4 -tx1 /dev/urandom | tr -d ' \n')"
    runner="$integration_prefix-runner:latest"
    integration_images=("$runner")
    # buildx keeps its state in ~/.docker, which a sandboxed worker may not write.
    export BUILDX_CONFIG="$out/.buildx"
    trap 'integration_cleanup; exit 143' TERM
    trap integration_cleanup EXIT
    step integration-runner integration_runner_image "$runtime" "$runner" || return 1
    # One at a time: parts of the vendored libraries' build write into the source tree.
    for net in "${!images[@]}"; do
        images[$net]="registry.gitlab.syncad.com/hive/hive$([ "$net" = testnet ] && echo /testnet):$integration_prefix"
        integration_images+=("${images[$net]}")
        rc=0
        step "integration-image-$net" hived_image "$net" "$runtime" "${images[$net]}" "$inline" || rc=$?
        [ "$rc" -eq 0 ] && continue
        for s in "${scripts[@]}"; do
            [ "$(integration_script_needs "$s" | cut -d' ' -f1)" = "$net" ] \
                && skip_case "integration-${s%.sh}" "its $net image was not built"
        done
        [ "$rc" -eq "$step_skipped" ] || failed=1
        unset "images[$net]"
    done
    for s in "${scripts[@]}"; do
        read -r net _ <<< "$(integration_script_needs "$s")"
        [ -n "${images[$net]:-}" ] || continue
        step "integration-${s%.sh}" integration_script "$s" "${images[$net]}" \
            $(integration_script_needs "$s" | cut -d' ' -f2-) "$runner" &
        pids+=("$!")
    done
    for pid in "${pids[@]}"; do
        wait "$pid"; rc=$?
        [ "$rc" -eq 0 ] || [ "$rc" -eq "$step_skipped" ] || failed=1
    done
    return "$failed"
}

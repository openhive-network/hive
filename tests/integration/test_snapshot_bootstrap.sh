#! /bin/bash

set -euo pipefail

################################################################################
# Test: snapshot bootstrap of the docker entrypoint (SNAPSHOT_URL)
################################################################################
#
# The entrypoint passes --load-snapshot-if-no-state whenever an unpacked snapshot
# exists, and downloads the snapshot only when hived would load it. These cases
# run a testnet image against a snapshot served from this host:
#
#   1. fresh volume: the snapshot is downloaded and loaded
#   2. restart: neither downloaded nor loaded again
#   3. state left by an older entrypoint (no hived marker, unpacked snapshot kept,
#      .snapshot_loaded of the previous entrypoint): not reloaded
#   4. unpacked snapshot deleted: the node starts without a snapshot option
#   5. load killed mid-way (hived's marker left at "loading"): loaded again
#   6. snapshot and block log: the block log is replayed forward from the
#      snapshot; a replay stopped half-way is resumed by the next start
#   7. --force-replay, also abbreviated (--force-rep): replays, no snapshot option
#      is added; the same for --resync
#   8. a refusal (invalid options, a replay owed on a state that is gone) is the
#      container's exit code (78)
#   9. a snapshot directory name with a space reaches hived as one argument
#  10. HIVED_BOOTSTRAP_ONLY=1 loads and exits, HIVED_SKIP_BOOTSTRAP=1 runs without
#      a snapshot option; a bootstrap against a running node exits 0 at once
#  11. shared_memory_file_util: valid details with head, LIB and hived's marker,
#      state files left byte-identical, a running node refused at once (repeatedly,
#      also while the node grows its state file), a state with other definitions refused
#  12. docker/exchange/compose.yml: a refused snapshot leaves hived-bootstrap exited
#      with 78 and hived never started; a good snapshot starts hived
#
# The snapshot is served by a container on a network of its own, so the test also
# runs against a Docker daemon on another host (docker-in-docker in CI); the work
# dir must then be on a path that daemon shares, such as the CI project directory.
# The served archives are packed inside that container, from the snapshot the daemon
# sees, so nothing this host writes into the work dir has to reach the server.
#
# Usage: test_snapshot_bootstrap.sh <testnet hived image> <work dir>
#   PYTHON_IMAGE: an image with python3, to serve the snapshot and check JSON
#   TOOL_RUNS: how many times shared_memory_file_util is run against a running node
#   MIN_RESIZES: how many times the running node must have grown its state file meanwhile
################################################################################

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <testnet hived image> <work dir>"
  exit 1
fi

IMAGE=$1
WORK_DIR=$(realpath -m "$2")
PYTHON_IMAGE=${PYTHON_IMAGE:-python:3.12-alpine}
TOOL_RUNS=${TOOL_RUNS:-200}
MIN_RESIZES=${MIN_RESIZES:-5}

SNAPSHOT_NAME=snap
SNAPSHOT_BLOCK=30
BLOCK_LOG_HEAD=50
# Testnet initminer key, the only witness of a fresh testnet.
INIT_KEY=5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
REFUSED_EXIT_CODE=78
NETWORK=snapshot-bootstrap
HTTP_SERVER=snapshot-bootstrap-http
PRODUCER=snapshot-bootstrap-producer
SNAPSHOT_URL="http://${HTTP_SERVER}:8000/${SNAPSHOT_NAME}.tgz"
MARKER=blockchain/snapshot_load_pending.json
TOOL=/home/hived/bin/shared_memory_file_util
COMPOSE_DIR="$WORK_DIR/compose"
COMPOSE_PROJECT=snapshot-bootstrap-compose
COMPOSE_IMAGE=snapshot-bootstrap-compose
SCRIPT_DIR=$(dirname "$(realpath "$0")")

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

compose() {
  docker compose --project-name "$COMPOSE_PROJECT" --project-directory "$COMPOSE_DIR" \
    --file "$COMPOSE_DIR/compose.yml" --file "$COMPOSE_DIR/compose.test.yml" "$@"
}

cleanup() {
  [[ ! -f "$COMPOSE_DIR/compose.test.yml" ]] || compose down --volumes > /dev/null 2>&1 || true
  docker rm -f "$HTTP_SERVER" "$PRODUCER" > /dev/null 2>&1 || true
  docker network rm "$NETWORK" > /dev/null 2>&1 || true
}
cleanup
trap cleanup EXIT
docker network create "$NETWORK" > /dev/null

fail() {
  echo "FAILED: $*"
  exit 1
}

# new_datadir NAME [extra config line...]: a data directory with a config.ini, writable by the container.
new_datadir() {
  local dir="$WORK_DIR/$1"; shift
  mkdir -p "$dir/blockchain"
  {
    echo "plugin = state_snapshot"
    echo "shared-file-size = 128M"
    local line
    for line in "$@"; do echo "$line"; done
  } > "$dir/config.ini"
  chmod -R a+rwX "$dir"
}

# run CASE DATADIR [docker option...] -- [hived argument...]: runs the image's entrypoint to completion;
# its output goes to $WORK_DIR/CASE.log and its exit code to $RC.
run() {
  local case_name="$1" dir="$WORK_DIR/$2"; shift 2
  local docker_options=()
  while [[ "$1" != "--" ]]; do docker_options+=("$1"); shift; done
  shift
  echo "=== ${case_name}"
  RC=0
  docker run --rm --name "snapshot-bootstrap-${case_name}" --network "$NETWORK" \
    --volume "$dir:/home/hived/datadir" "${docker_options[@]}" "$IMAGE" "$@" \
    > "$WORK_DIR/${case_name}.log" 2>&1 || RC=$?
  echo "exit code: $RC"
}

# tool CASE DATADIR [tool argument...]: runs shared_memory_file_util on the state of DATADIR; its stdout goes to
# $WORK_DIR/CASE.json, its stderr to $WORK_DIR/CASE.log and its exit code to $RC.
tool() {
  local case_name="$1" dir="$WORK_DIR/$2"; shift 2
  echo "=== ${case_name}"
  RC=0
  docker run --rm --entrypoint "$TOOL" --volume "$dir:/home/hived/datadir" "$IMAGE" --input /home/hived/datadir/blockchain "$@" \
    > "$WORK_DIR/${case_name}.json" 2> "$WORK_DIR/${case_name}.log" || RC=$?
  echo "exit code: $RC"
}

expect_rc() { [[ "$RC" -eq "$1" ]] || fail "$CASE: exit code $RC, expected $1 (see $WORK_DIR/$CASE.log)"; }
expect_log() { grep -qF -- "$1" "$WORK_DIR/$CASE.log" || fail "$CASE: no '$1' in its log"; }
expect_no_log() { ! grep -qF -- "$1" "$WORK_DIR/$CASE.log" || fail "$CASE: unexpected '$1' in its log"; }
expect_no_marker() { [[ ! -e "$WORK_DIR/$1/$MARKER" ]] || fail "$CASE: hived's marker left: $(cat "$WORK_DIR/$1/$MARKER")"; }
# The arguments the entrypoint passes to hived, as it logs them.
hived_args() { grep -F -- "Attempting to execute hived using additional command line arguments:" "$WORK_DIR/$CASE.log" || true; }
expect_args() { grep -qF -- "$1" < <(hived_args) || fail "$CASE: no '$1' among hived's arguments"; }
expect_no_args() { ! grep -qF -- "$1" < <(hived_args) || fail "$CASE: '$1' among hived's arguments"; }

# expect_details PENDING: the details printed by the tool are valid JSON, with a head block, a LIB not above it, and
# snapshot_load_pending equal to PENDING (JSON).
expect_details() {
  docker run --rm -i "$PYTHON_IMAGE" python3 -c '
import json, sys
details = json.load(sys.stdin)
head = details["head_block"]["number"]
lib = details["last_irreversible_block"]["_irreversible_block_num"]
assert 0 < lib <= head, (lib, head)
assert details["snapshot_load_pending"] == json.loads(sys.argv[1]), details["snapshot_load_pending"]
' "$1" < "$WORK_DIR/$CASE.json" || fail "$CASE: unexpected details: $(cat "$WORK_DIR/$CASE.json")"
}

# Hashes of the files under the blockchain directory of a data directory, except logs.
state_hashes() { (cd "$WORK_DIR/$1/blockchain" && find . -type f ! -name '*.log' -print0 | sort -z | xargs -0 sha256sum); }

expect_loaded() {
  expect_rc 0
  expect_log "loading snapshot \`${SNAPSHOT_NAME}'"
  expect_log "Wiping DB to load the snapshot."
  expect_log "Snapshot load completed"
}
expect_kept() {
  expect_rc 0
  expect_no_log "Downloading"
  expect_log "Complete state found, snapshot \`${SNAPSHOT_NAME}' is not loaded."
  expect_no_log "Requested to clear shared memory data"
  expect_no_log "Wiping DB to load the snapshot."
}

######### Source chain and snapshot #########
new_datadir source "plugin = witness" "witness = \"initminer\"" "private-key = ${INIT_KEY}" \
  "enable-stale-production = true" "required-participation = 0"
CASE=produce; run "$CASE" source -- --exit-at-block="$SNAPSHOT_BLOCK"; expect_rc 0
CASE=dump; run "$CASE" source -- --dump-snapshot="$SNAPSHOT_NAME" --exit-before-sync; expect_rc 0
CASE=produce-more; run "$CASE" source -- --exit-at-block="$BLOCK_LOG_HEAD"; expect_rc 0
# broken.tgz: a snapshot directory with no snapshot in it, which hived refuses to load.
docker run -d --name "$HTTP_SERVER" --network "$NETWORK" --volume "$WORK_DIR/source/snapshot:/snapshot:ro" \
  "$PYTHON_IMAGE" sh -ec "
    mkdir -p /tmp/www /tmp/broken/broken
    tar -czf /tmp/www/${SNAPSHOT_NAME}.tgz -C /snapshot ${SNAPSHOT_NAME}
    echo 'not a snapshot' > /tmp/broken/broken/README
    tar -czf /tmp/www/broken.tgz -C /tmp/broken broken
    exec python3 -m http.server --directory /tmp/www 8000" > /dev/null
for _ in $(seq 30); do
  docker run --rm --network "$NETWORK" --entrypoint wget "$IMAGE" -q --spider "$SNAPSHOT_URL" 2> /dev/null && break
  sleep 1
done
docker run --rm --network "$NETWORK" --entrypoint wget "$IMAGE" -q --spider "$SNAPSHOT_URL" \
  || fail "the snapshot is not served: $(docker logs "$HTTP_SERVER" 2>&1 | tail -20)"

SNAPSHOT_ENV=(--env "SNAPSHOT_URL=$SNAPSHOT_URL")

######### Snapshot only #########
new_datadir node

CASE=fresh-volume; run "$CASE" node "${SNAPSHOT_ENV[@]}" -- --exit-before-sync
expect_loaded
expect_log "Downloading ${SNAPSHOT_URL}"
expect_log "No state found"
expect_no_marker node

CASE=restart; run "$CASE" node "${SNAPSHOT_ENV[@]}" -- --exit-before-sync
expect_kept

CASE=older-entrypoint-state; touch "$WORK_DIR/node/.snapshot_loaded"
run "$CASE" node "${SNAPSHOT_ENV[@]}" -- --exit-before-sync
expect_kept
[[ ! -e "$WORK_DIR/node/.snapshot_loaded" ]] || fail "$CASE: .snapshot_loaded of the older entrypoint kept"

CASE=snapshot-deleted; rm -rf "$WORK_DIR/node/snapshot"
run "$CASE" node "${SNAPSHOT_ENV[@]}" -- --exit-before-sync
expect_rc 0
expect_no_log "Downloading"
expect_no_log "--load-snapshot-if-no-state"
expect_no_log "Requested to clear shared memory data"

CASE=killed-load
echo "{\"stage\":\"loading\",\"snapshot\":\"${SNAPSHOT_NAME}\"}" > "$WORK_DIR/node/$MARKER"
run "$CASE" node "${SNAPSHOT_ENV[@]}" -- --exit-before-sync
expect_loaded
expect_log "Downloading ${SNAPSHOT_URL}"
expect_log "did not complete, loading snapshot"
expect_no_marker node

CASE=restart-after-killed-load; run "$CASE" node "${SNAPSHOT_ENV[@]}" -- --exit-before-sync
expect_kept

######### Snapshot and block log #########
new_datadir node-with-block-log
cp "$WORK_DIR"/source/blockchain/block_log* "$WORK_DIR/node-with-block-log/blockchain/"
chmod -R a+rwX "$WORK_DIR/node-with-block-log"

CASE=replay-stopped; run "$CASE" node-with-block-log "${SNAPSHOT_ENV[@]}" -- --exit-at-block=$((SNAPSHOT_BLOCK + 5))
expect_rc 0
expect_log "Wiping DB to load the snapshot."
grep -qF '"stage":"replaying"' "$WORK_DIR/node-with-block-log/$MARKER" || fail "$CASE: no marker of the replay owed"

CASE=tool-details-with-marker; tool "$CASE" node-with-block-log --get-details
expect_rc 0
expect_details "{\"stage\":\"replaying\",\"snapshot\":\"${SNAPSHOT_NAME}\"}"

CASE=replay-resumed; run "$CASE" node-with-block-log "${SNAPSHOT_ENV[@]}" -- --exit-before-sync
expect_rc 0
expect_no_log "Downloading"
expect_no_log "Wiping DB to load the snapshot."
expect_log "resuming it"
expect_log "Snapshot load completed"
expect_no_marker node-with-block-log

CASE=force-replay; run "$CASE" node-with-block-log "${SNAPSHOT_ENV[@]}" -- --force-replay --exit-before-sync
expect_rc 0
expect_no_log "--load-snapshot-if-no-state"
expect_no_log "Wiping DB to load the snapshot."
expect_log "Done reindexing"

CASE=abbreviated-force-replay; run "$CASE" node-with-block-log "${SNAPSHOT_ENV[@]}" -- --force-rep --exit-before-sync
expect_rc 0
expect_no_args "--load-snapshot-if-no-state"
expect_no_log "Wiping DB to load the snapshot."
expect_log "Done reindexing"

######### Refusals #########
CASE=refusal; run "$CASE" node-with-block-log -- --load-snapshot-if-no-state="$SNAPSHOT_NAME" --force-replay
expect_rc "$REFUSED_EXIT_CODE"
expect_log "--load-snapshot-if-no-state cannot be combined"

CASE=bootstrap-only-refusal
run "$CASE" node-with-block-log --env HIVED_BOOTSTRAP_ONLY=1 -- --load-snapshot-if-no-state="$SNAPSHOT_NAME" --force-replay
expect_rc "$REFUSED_EXIT_CODE"
expect_log "--load-snapshot-if-no-state cannot be combined"

# a replay owed after a load whose state is gone would otherwise start from genesis
CASE=replay-owed-without-state
cp -a "$WORK_DIR/node-with-block-log" "$WORK_DIR/replay-owed"
rm "$WORK_DIR/replay-owed/blockchain/shared_memory.bin"
echo "{\"stage\":\"replaying\",\"snapshot\":\"${SNAPSHOT_NAME}\"}" > "$WORK_DIR/replay-owed/$MARKER"
run "$CASE" replay-owed -- --exit-before-sync
expect_rc "$REFUSED_EXIT_CODE"
expect_log "no state to resume it on"
expect_log "--force-replay"
[[ ! -e "$WORK_DIR/replay-owed/blockchain/shared_memory.bin" ]] || fail "$CASE: a state was created"

CASE=abbreviated-resync; run "$CASE" replay-owed "${SNAPSHOT_ENV[@]}" -- --resync --exit-before-sync
expect_rc 0
expect_no_args "--load-snapshot-if-no-state"
expect_log "resync requested"

######### Arguments with spaces #########
new_datadir quoted
mkdir -p "$WORK_DIR/quoted/snapshot"
cp -a "$WORK_DIR/source/snapshot/$SNAPSHOT_NAME" "$WORK_DIR/quoted/snapshot/snap shot"
chmod -R a+rwX "$WORK_DIR/quoted"
CASE=snapshot-name-with-space; run "$CASE" quoted "${SNAPSHOT_ENV[@]}" -- --exit-before-sync
expect_rc 0
expect_no_log "Downloading"
expect_log "loading snapshot \`snap shot'"
expect_log "Snapshot load completed"

######### Bootstrap modes #########
new_datadir split

CASE=bootstrap-only; run "$CASE" split "${SNAPSHOT_ENV[@]}" --env HIVED_BOOTSTRAP_ONLY=1 --
expect_loaded
expect_args "--exit-before-sync"
expect_no_marker split

CASE=skip-bootstrap; run "$CASE" split "${SNAPSHOT_ENV[@]}" --env HIVED_SKIP_BOOTSTRAP=1 -- --exit-before-sync
expect_rc 0
expect_no_log "Downloading"
expect_no_args "--load-snapshot-if-no-state"
expect_args "--block-log-split=0"

CASE=bootstrap-only-again; run "$CASE" split "${SNAPSHOT_ENV[@]}" --env HIVED_BOOTSTRAP_ONLY=1 --
expect_kept

######### shared_memory_file_util #########
CASE=tool-details; tool "$CASE" source --get-details
expect_rc 0
expect_details null

CASE=tool-leaves-state-untouched
hashes_before=$(state_hashes source)
tool "$CASE" source --get-all-data --output /tmp/shm_data
expect_rc 0
[[ "$hashes_before" == "$(state_hashes source)" ]] || fail "$CASE: the state files changed"

CASE=tool-other-definitions
cp -a "$WORK_DIR/source" "$WORK_DIR/altered"
# Changes one digit of the checksum stored for a type the tool reads: the same length, so the rest of the file is intact.
alter_definitions='
import re, sys
path = sys.argv[1]
with open(path, "rb") as f:
    data = bytearray(f.read())
pattern = re.compile(rb"\"name\":\"hive::chain::dynamic_global_property_object\",\"size_of\":[0-9]+,\"align_of\":[0-9]+,\"checksum\":\"([0-9a-f])")
matches = list(pattern.finditer(data))
assert matches, "no definition of dynamic_global_property_object"
for match in matches:
    data[match.start(1)] = ord("1") if data[match.start(1)] == ord("0") else ord("0")
with open(path, "wb") as f:
    f.write(data)
'
docker run --rm --volume "$WORK_DIR/altered/blockchain:/state" "$PYTHON_IMAGE" python3 -c "$alter_definitions" /state/shared_memory.bin
tool "$CASE" altered --get-details
[[ "$RC" -ne 0 ]] || fail "$CASE: a state with other definitions was read"
expect_log "do not match this build"

# A node producing blocks holds the state: the tool is refused at once, never left waiting on a mutex in the state.
# The node grows its state file after every block, reopening it each time, and keeps the state locked meanwhile.
docker run -d --name "$PRODUCER" --network "$NETWORK" --volume "$WORK_DIR/source:/home/hived/datadir" "$IMAGE" \
  --shared-file-full-threshold=1 --shared-file-scale-rate=100 > /dev/null
for _ in $(seq 60); do
  grep -qF "Generated block" < <(docker logs "$PRODUCER" 2>&1) && break
  sleep 1
done
grep -qF "Generated block" < <(docker logs "$PRODUCER" 2>&1) || fail "the producer does not produce blocks"

resizes() { docker logs "$PRODUCER" 2>&1 | grep -cF "Memory is almost full, increasing" || true; }

CASE=tool-running-node
echo "=== ${CASE}: at least ${TOOL_RUNS} runs and ${MIN_RESIZES} resizes of the node's state"
i=0
deadline=$((SECONDS + 300))
while (( i < TOOL_RUNS || $(resizes) < MIN_RESIZES )); do
  (( SECONDS < deadline )) || fail "$CASE: the node grew its state $(resizes) times in 300s, expected ${MIN_RESIZES}"
  i=$((i + 1))
  RC=0
  timeout 30 docker exec "$PRODUCER" "$TOOL" --input /home/hived/datadir/blockchain --get-details > /dev/null 2> "$WORK_DIR/$CASE.log" || RC=$?
  [[ "$RC" -ne 124 ]] || fail "$CASE: run $i hangs"
  [[ "$RC" -ne 0 ]] || fail "$CASE: run $i read the state of a running node"
  expect_log "in use by another process"
done
echo "${i} runs, $(resizes) resizes"
docker logs "$PRODUCER" > "$WORK_DIR/producer.log" 2>&1
[[ "$(docker inspect --format '{{.State.Running}}' "$PRODUCER")" == "true" ]] || fail "$CASE: the node stopped (see $WORK_DIR/producer.log)"
! grep -qF "could not gain write access" "$WORK_DIR/producer.log" || fail "$CASE: the node lost its state while growing it"

CASE=bootstrap-only-on-running-node; run "$CASE" source --env HIVED_BOOTSTRAP_ONLY=1 --
expect_rc 0
expect_log "a running hived holds the state"
docker stop "$PRODUCER" > /dev/null

######### docker compose #########
docker compose version > /dev/null || fail "docker compose is not available"
mkdir -p "$COMPOSE_DIR"
cp "$SCRIPT_DIR/../../docker/exchange/compose.yml" "$COMPOSE_DIR/"
printf '%s\n' "plugin = state_snapshot" "shared-file-size = 128M" > "$COMPOSE_DIR/config.ini"
chmod a+r "$COMPOSE_DIR/config.ini"
# The services reach the snapshot server, and publish no ports on this host. Under AIDEV they carry its owner label
# and OOM score, as .aidev/docker-shim/docker gives the containers `docker run` starts.
service_options=""
[[ -z "${AIDEV_ENV_CONTAINER_OWNER:-}" ]] \
  || service_options=$'\n    labels: ["aidev.env-container.owner=${AIDEV_ENV_CONTAINER_OWNER}"]\n    oom_score_adj: 900'
cat > "$COMPOSE_DIR/compose.test.yml" <<COMPOSE
networks:
  snapshot:
    name: ${NETWORK}
    external: true
services:
  hived-bootstrap:
    networks: [default, snapshot]${service_options}
  hived:
    networks: [default, snapshot]
    ports: !reset []${service_options}
COMPOSE
docker tag "$IMAGE" "$COMPOSE_IMAGE:test"

# compose_up CASE SNAPSHOT_URL: `docker compose up -d` on fresh volumes; its output and the bootstrap's log go to
# $WORK_DIR/CASE.log and its exit code to $RC.
compose_up() {
  local case_name="$1"
  echo "=== ${case_name}"
  compose down --volumes > /dev/null 2>&1 || true
  printf '%s\n' "HIVED_IMAGE=${COMPOSE_IMAGE}" "HIVED_VERSION=test" "SNAPSHOT_URL=$2" > "$COMPOSE_DIR/.env"
  RC=0
  compose up -d > "$WORK_DIR/${case_name}.log" 2>&1 || RC=$?
  compose logs hived-bootstrap >> "$WORK_DIR/${case_name}.log" 2>&1 || true
  echo "exit code: $RC"
}
# The container of a compose service, empty when it was never created.
service_container() { compose ps --all --quiet "$1"; }

CASE=compose-refused-snapshot; compose_up "$CASE" "http://${HTTP_SERVER}:8000/broken.tgz"
[[ "$RC" -ne 0 ]] || fail "$CASE: docker compose up succeeded"
expect_log "cannot be loaded"
bootstrap=$(service_container hived-bootstrap)
[[ -n "$bootstrap" ]] || fail "$CASE: no hived-bootstrap container"
bootstrap_rc=$(docker inspect --format '{{.State.ExitCode}}' "$bootstrap")
[[ "$bootstrap_rc" -eq "$REFUSED_EXIT_CODE" ]] || fail "$CASE: hived-bootstrap exit code $bootstrap_rc, expected $REFUSED_EXIT_CODE"
node=$(service_container hived)
[[ -z "$node" || "$(docker inspect --format '{{.State.StartedAt}}' "$node")" == 0001-01-01T00:00:00* ]] || fail "$CASE: hived was started"

CASE=compose-snapshot; compose_up "$CASE" "$SNAPSHOT_URL"
expect_rc 0
expect_log "Snapshot load completed"
node=$(service_container hived)
[[ -n "$node" ]] || fail "$CASE: no hived container"
for _ in $(seq 60); do
  grep -qF "Started on blockchain with" < <(docker logs "$node" 2>&1) && break
  sleep 1
done
docker logs "$node" > "$WORK_DIR/${CASE}-hived.log" 2>&1
grep -qF "Started on blockchain with" "$WORK_DIR/${CASE}-hived.log" || fail "$CASE: hived did not start (see $WORK_DIR/${CASE}-hived.log)"
[[ "$(docker inspect --format '{{.State.Running}}' "$node")" == "true" ]] || fail "$CASE: hived is not running"
compose down --volumes > /dev/null 2>&1

echo "Test passed!"

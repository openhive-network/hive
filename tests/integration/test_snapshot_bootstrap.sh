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
#   7. --force-replay: replays, no snapshot option is added
#   8. a refusal (invalid options) is the container's exit code (78)
#
# Usage: test_snapshot_bootstrap.sh <testnet hived image> <work dir>
################################################################################

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <testnet hived image> <work dir>"
  exit 1
fi

IMAGE=$1
WORK_DIR=$(realpath -m "$2")

SNAPSHOT_NAME=snap
SNAPSHOT_BLOCK=30
BLOCK_LOG_HEAD=50
# Testnet initminer key, the only witness of a fresh testnet.
INIT_KEY=5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
REFUSED_EXIT_CODE=78
PORT=${SNAPSHOT_HTTP_PORT:-18765}
SNAPSHOT_URL="http://host.docker.internal:${PORT}/${SNAPSHOT_NAME}.tgz"
MARKER=blockchain/snapshot_load_pending.json

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/www"

HTTP_PID=""
cleanup() {
  [[ -z "$HTTP_PID" ]] || kill "$HTTP_PID" 2>/dev/null || true
}
trap cleanup EXIT

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
  docker run --rm --name "snapshot-bootstrap-${case_name}" --add-host=host.docker.internal:host-gateway \
    --volume "$dir:/home/hived/datadir" "${docker_options[@]}" "$IMAGE" "$@" \
    > "$WORK_DIR/${case_name}.log" 2>&1 || RC=$?
  echo "exit code: $RC"
}

expect_rc() { [[ "$RC" -eq "$1" ]] || fail "$CASE: exit code $RC, expected $1 (see $WORK_DIR/$CASE.log)"; }
expect_log() { grep -qF -- "$1" "$WORK_DIR/$CASE.log" || fail "$CASE: no '$1' in its log"; }
expect_no_log() { ! grep -qF -- "$1" "$WORK_DIR/$CASE.log" || fail "$CASE: unexpected '$1' in its log"; }
expect_no_marker() { [[ ! -e "$WORK_DIR/$1/$MARKER" ]] || fail "$CASE: hived's marker left: $(cat "$WORK_DIR/$1/$MARKER")"; }

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
tar --create --gzip --file="$WORK_DIR/www/${SNAPSHOT_NAME}.tgz" --directory="$WORK_DIR/source/snapshot" "$SNAPSHOT_NAME"
python3 -m http.server --bind 0.0.0.0 --directory "$WORK_DIR/www" "$PORT" > "$WORK_DIR/http.log" 2>&1 &
HTTP_PID=$!
sleep 2

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

######### Refusal #########
CASE=refusal; run "$CASE" node-with-block-log -- --load-snapshot-if-no-state="$SNAPSHOT_NAME" --force-replay
expect_rc "$REFUSED_EXIT_CODE"
expect_log "--load-snapshot-if-no-state cannot be combined"

echo "Test passed!"

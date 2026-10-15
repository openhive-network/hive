#! /bin/bash

set -euo pipefail

################################################################################
# Test: HIVED_UID of the docker entrypoint
################################################################################
#
# hived in the image runs as hived (UID 1000). HIVED_UID makes it run as another
# UID, so that bind-mounted directories owned by that UID need no 777 mode:
#
#   1. HIVED_UID=1234, datadir and shm_dir bind-mounted from directories owned by
#      1234 with mode 755: hived runs as 1234 and creates its files there; a
#      docker stop still ends it cleanly (exit code 0)
#   2. the same directories without HIVED_UID: refused at datadir creation
#   3. no HIVED_UID, the image's own volumes: hived runs as 1000, as before; the
#      same with HIVED_UID=1000, which remaps nothing
#   4. HIVED_UID=0, abc, 01234: refused with an explicit message
#   5. hived's exit code is the container's after a remap (a refusal, 78)
#
# Each case stops as soon as hived has created its state file; nothing is synced.
# The directories are prepared and removed by containers of the image, so the
# script needs no root; the work dir must be on a path the Docker daemon shares.
#
# Usage: test_hived_uid.sh <testnet hived image> <work dir>
################################################################################

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <testnet hived image> <work dir>"
  exit 1
fi

IMAGE=$1
WORK_DIR=$(realpath -m "$2")

OWNER_UID=1234
DEFAULT_UID=1000
REFUSED_EXIT_CODE=78
CONTAINER=hived-uid-test
STATE_FILE_TIMEOUT=120

# as_root COMMAND...: runs a shell command as root in a container of the image, with the work dir mounted at itself.
as_root() {
  docker run --rm --user 0 --entrypoint bash --volume "$WORK_DIR:$WORK_DIR" "$IMAGE" -c "$1"
}

cleanup() {
  docker rm -f "$CONTAINER" > /dev/null 2>&1 || true
  # The data files are hived's and may be owned by a UID other than this script's; only the logs stay.
  [[ ! -d "$WORK_DIR" ]] || as_root "find '$WORK_DIR' -mindepth 1 ! -name '*.log' -delete" > /dev/null 2>&1 || true
}

mkdir -p "$WORK_DIR"
cleanup
trap cleanup EXIT

fail() {
  echo "FAILED: $*"
  exit 1
}

# owned_dirs: data and shm directories under $WORK_DIR owned by $OWNER_UID with mode 755, and a config.ini.
owned_dirs() {
  as_root "rm -rf '$WORK_DIR/datadir' '$WORK_DIR/shm' && mkdir -p '$WORK_DIR/datadir' '$WORK_DIR/shm' \
    && printf 'shared-file-size = 128M\n' > '$WORK_DIR/datadir/config.ini' \
    && chown -R $OWNER_UID:100 '$WORK_DIR/datadir' '$WORK_DIR/shm' && chmod 755 '$WORK_DIR/datadir' '$WORK_DIR/shm' \
    && chmod 644 '$WORK_DIR/datadir/config.ini'"
}
OWNED_VOLUMES=(--volume "$WORK_DIR/datadir:/home/hived/datadir" --volume "$WORK_DIR/shm:/home/hived/shm_dir"
  --env SHM_DIR=/home/hived/shm_dir)

# run CASE [docker option...] -- [hived argument...]: runs the image's entrypoint to completion; its output goes
# to $WORK_DIR/CASE.log and its exit code to $RC.
run() {
  local case_name="$1"; shift
  local docker_options=()
  while [[ "$1" != "--" ]]; do docker_options+=("$1"); shift; done
  shift
  echo "=== ${case_name}"
  RC=0
  docker run --rm --name "$CONTAINER" "${docker_options[@]}" "$IMAGE" "$@" > "$WORK_DIR/${case_name}.log" 2>&1 || RC=$?
  echo "exit code: $RC"
}

# run_until_state CASE STATE_FILE [docker option...]: starts the image's entrypoint, waits until hived has created
# STATE_FILE (a path in the container), records the UID hived runs as in $HIVED_PROCESS_UID, and stops the container;
# its output goes to $WORK_DIR/CASE.log and its exit code to $RC.
run_until_state() {
  local case_name="$1" state_file="$2"; shift 2
  echo "=== ${case_name}"
  docker run -d --name "$CONTAINER" "$@" "$IMAGE" --shared-file-size=128M > /dev/null
  local waited=0
  until docker exec "$CONTAINER" test -f "$state_file" 2> /dev/null; do
    if [[ "$(docker inspect --format '{{.State.Running}}' "$CONTAINER")" != true ]]; then
      docker logs "$CONTAINER" > "$WORK_DIR/${case_name}.log" 2>&1
      fail "$case_name: the container exited before hived created $state_file (see $WORK_DIR/${case_name}.log)"
    fi
    (( waited++ < STATE_FILE_TIMEOUT )) || fail "$case_name: no $state_file after ${STATE_FILE_TIMEOUT}s"
    sleep 1
  done
  HIVED_PROCESS_UID=$(docker exec "$CONTAINER" ps -o uid= -C hived | tr -d ' ')
  docker stop "$CONTAINER" > /dev/null
  RC=$(docker inspect --format '{{.State.ExitCode}}' "$CONTAINER")
  docker logs "$CONTAINER" > "$WORK_DIR/${case_name}.log" 2>&1
  docker rm "$CONTAINER" > /dev/null
  echo "hived UID: $HIVED_PROCESS_UID, exit code: $RC"
}

expect_rc() { [[ "$RC" -eq "$1" ]] || fail "$CASE: exit code $RC, expected $1 (see $WORK_DIR/$CASE.log)"; }
expect_log() { grep -qF -- "$1" "$WORK_DIR/$CASE.log" || fail "$CASE: no '$1' in its log"; }
expect_no_log() { ! grep -qF -- "$1" "$WORK_DIR/$CASE.log" || fail "$CASE: unexpected '$1' in its log"; }
expect_uid() { [[ "$HIVED_PROCESS_UID" == "$1" ]] || fail "$CASE: hived runs as UID '$HIVED_PROCESS_UID', expected $1"; }
# expect_owned PATH UID: PATH under $WORK_DIR exists and is owned by UID.
expect_owned() {
  local owner
  owner=$(as_root "stat -c %u '$WORK_DIR/$1'" 2> /dev/null) || fail "$CASE: no $1"
  [[ "$owner" == "$2" ]] || fail "$CASE: $1 is owned by UID $owner, expected $2"
}

######### Bind mounts owned by another UID #########
owned_dirs
CASE=remapped; run_until_state "$CASE" /home/hived/shm_dir/shared_memory.bin "${OWNED_VOLUMES[@]}" --env HIVED_UID=$OWNER_UID
expect_rc 0
expect_uid $OWNER_UID
expect_log "Remapping hived UID to $OWNER_UID"
expect_owned datadir/blockchain $OWNER_UID
expect_owned shm/shared_memory.bin $OWNER_UID

owned_dirs
CASE=not-remapped; run "$CASE" "${OWNED_VOLUMES[@]}" -- --exit-before-sync
expect_rc 1
expect_log "Permission denied"
expect_no_log "Remapping hived UID"

######### The image's own volumes #########
CASE=default; run_until_state "$CASE" /home/hived/datadir/blockchain/shared_memory.bin
expect_rc 0
expect_uid $DEFAULT_UID
expect_no_log "Remapping hived UID"

CASE=same-uid; run_until_state "$CASE" /home/hived/datadir/blockchain/shared_memory.bin --env HIVED_UID=$DEFAULT_UID
expect_rc 0
expect_uid $DEFAULT_UID
expect_no_log "Remapping hived UID"

######### Invalid values #########
for value in 0 abc 01234; do
  CASE="invalid-$value"; run "$CASE" --env "HIVED_UID=$value" -- --exit-before-sync
  expect_rc 1
  expect_log "HIVED_UID must be a non-zero numeric UID without leading zeros, got '$value'. Exiting."
  expect_no_log "Remapping hived UID"
done

######### Exit code through the remap #########
owned_dirs
CASE=remapped-refusal; run "$CASE" "${OWNED_VOLUMES[@]}" --env HIVED_UID=$OWNER_UID -- --load-snapshot-if-no-state=snap --force-replay
expect_rc $REFUSED_EXIT_CODE
expect_log "--load-snapshot-if-no-state cannot be combined"
expect_log "Remapping hived UID to $OWNER_UID"

echo "All HIVED_UID tests passed."

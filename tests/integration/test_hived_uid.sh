#! /bin/bash

set -euo pipefail

################################################################################
# Test: HIVED_UID of the docker entrypoint
################################################################################
#
# hived in the image runs as UID 1000. HIVED_UID makes it run as another UID, so
# it can use bind-mounted directories owned by that UID without opening them up.
# These cases run a testnet image; each one ends as soon as hived has created its
# files, no sync is needed:
#
#   1. HIVED_UID set, datadir and shm_dir bind-mounted from directories owned by
#      that UID with mode 755: hived runs as that UID and creates blockchain/ and
#      shared_memory.bin; the container stops cleanly and starts again as that UID
#   2. the same mounts without HIVED_UID: refused at datadir creation
#   3. no HIVED_UID, the image's own volumes: hived runs as 1000 as before
#   4. an invalid HIVED_UID (0, not numeric) is refused with a message
#
# The bind-mounted directories are created through a container, so the test also
# runs against a Docker daemon on another host (docker-in-docker in CI); the work
# dir must then be on a path that daemon shares, such as the CI project directory.
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
CONTAINER=hived-uid-test
START_TIMEOUT=120

# Runs a command as root in a throwaway container with the work dir mounted.
as_root() {
  docker run --rm --user 0 --entrypoint bash -v "$WORK_DIR:/work" "$IMAGE" -c "$1"
}

cleanup() {
  docker rm -f "$CONTAINER" >/dev/null 2>&1 || true
  if [[ -d "$WORK_DIR/mounts" ]]; then
    as_root 'rm -rf /work/mounts' || true
  fi
}
trap cleanup EXIT

fail() {
  echo "FAIL: $*"
  docker logs "$CONTAINER" 2>&1 | tail -50 || true
  exit 1
}

mkdir -p "$WORK_DIR"
cleanup

# Fresh datadir and shm_dir owned by OWNER_UID, mode 755.
prepare_mounts() {
  as_root "rm -rf /work/mounts && mkdir -p /work/mounts/datadir /work/mounts/shm_dir \
    && chown -R ${OWNER_UID}:${OWNER_UID} /work/mounts && chmod 755 /work/mounts/datadir /work/mounts/shm_dir"
}

MOUNT_ARGS=(
  -v "$WORK_DIR/mounts/datadir:/home/hived/datadir"
  -v "$WORK_DIR/mounts/shm_dir:/home/hived/shm_dir"
  -e SHM_DIR=/home/hived/shm_dir
)

# Waits until hived has created its state file in SHM_DIR (inside the container).
wait_for_state_file() {
  local shm_dir="$1" waited=0
  until docker exec "$CONTAINER" test -f "$shm_dir/shared_memory.bin" 2>/dev/null; do
    [[ "$(docker inspect -f '{{.State.Running}}' "$CONTAINER")" == "true" ]] || fail "container exited before hived created its state file"
    (( waited < START_TIMEOUT )) || fail "hived created no state file in ${START_TIMEOUT}s"
    sleep 1
    waited=$(( waited + 1 ))
  done
}

expect_hived_uid() {
  local expected="$1" actual
  actual=$(docker exec "$CONTAINER" ps -o uid= -C hived | tr -d ' ')
  [[ "$actual" == "$expected" ]] || fail "hived runs as UID '${actual}', expected ${expected}"
  echo "hived runs as UID ${actual}"
}

stop_cleanly() {
  docker stop -t 120 "$CONTAINER" >/dev/null
  local code
  code=$(docker inspect -f '{{.State.ExitCode}}' "$CONTAINER")
  [[ "$code" == 0 ]] || fail "container exited with ${code} on stop"
}

echo "=== 1. HIVED_UID=${OWNER_UID}, bind mounts owned by ${OWNER_UID} (mode 755) ==="
prepare_mounts
docker run -d --name "$CONTAINER" -e HIVED_UID="$OWNER_UID" "${MOUNT_ARGS[@]}" "$IMAGE" >/dev/null
wait_for_state_file /home/hived/shm_dir
expect_hived_uid "$OWNER_UID"
docker exec "$CONTAINER" test -d /home/hived/datadir/blockchain || fail "no blockchain/ in the datadir"
stop_cleanly
owners=$(as_root "stat -c %u /work/mounts/datadir/blockchain /work/mounts/shm_dir/shared_memory.bin" | sort -u)
[[ "$owners" == "$OWNER_UID" ]] || fail "hived's files are owned by '${owners}', expected ${OWNER_UID}"

echo "=== 1b. the same container started again ==="
docker start "$CONTAINER" >/dev/null
wait_for_state_file /home/hived/shm_dir
expect_hived_uid "$OWNER_UID"
stop_cleanly
docker rm "$CONTAINER" >/dev/null

echo "=== 2. no HIVED_UID, the same bind mounts ==="
prepare_mounts
status=0
docker run --name "$CONTAINER" "${MOUNT_ARGS[@]}" "$IMAGE" > "$WORK_DIR/no_uid.log" 2>&1 || status=$?
cat "$WORK_DIR/no_uid.log"
(( status != 0 )) || fail "the container started without HIVED_UID on directories it cannot write"
grep -q "^mkdir: .*Permission denied" "$WORK_DIR/no_uid.log" \
  || fail "no permission error at datadir creation"
docker rm "$CONTAINER" >/dev/null

echo "=== 3. no HIVED_UID, the image's own volumes ==="
docker run -d --name "$CONTAINER" "$IMAGE" >/dev/null
wait_for_state_file /home/hived/datadir/blockchain
expect_hived_uid "$DEFAULT_UID"
stop_cleanly
docker rm -v "$CONTAINER" >/dev/null

echo "=== 4. invalid HIVED_UID ==="
for uid in 0 abc; do
  status=0
  output=$(docker run --rm -e HIVED_UID="$uid" "$IMAGE" 2>&1) || status=$?
  echo "$output"
  (( status != 0 )) || fail "HIVED_UID=${uid} was accepted"
  grep -qF "HIVED_UID must be a non-zero numeric UID, not '${uid}'" <<< "$output" \
    || fail "HIVED_UID=${uid} was refused without the expected message"
done

echo "All HIVED_UID cases passed."

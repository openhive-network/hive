#! /bin/bash

set -euo pipefail

# Optional UID override: if HIVED_UID is set, the hived account is remapped to that
# UID and the entrypoint runs again as it, so hived can use bind-mounted volumes owned
# by a UID other than 1000. Remapping the account does not change the UID of a running
# process, and sudo refuses a process whose UID the account no longer has, so the
# entrypoint first runs itself again as root, remaps the account (usermod also hands
# over the files in /home/hived) and drops to it. sudo resets the environment, so it
# is passed on explicitly. On a restart the container already starts as the new UID.
if [[ -n "${HIVED_UID:-}" ]]; then
  if [[ ! "${HIVED_UID}" =~ ^[0-9]+$ ]] || (( 10#${HIVED_UID} == 0 )); then
    echo "HIVED_UID must be a non-zero numeric UID, not '${HIVED_UID}'. Exiting."
    exit 1
  fi
  hived_uid=$(( 10#${HIVED_UID} ))
  if (( hived_uid != $(id -u) )); then
    if (( $(id -u) != 0 )); then
      mapfile -d '' -t environment < <(env -0)
      exec sudo -n -- /usr/bin/env -i "${environment[@]}" "$0" "$@"
    fi
    echo "Remapping hived UID to ${hived_uid}"
    usermod -o -u "${hived_uid}" hived
    exec setpriv --reuid=hived --regid=users --init-groups -- "$0" "$@"
  fi
fi

SCRIPTDIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTDIR/scripts"

# Copy datadir from cache (CI only - DATA_SOURCE is not set in production)
if [ -n "${DATA_SOURCE+x}" ]; then
    # COMMON_CI_REF selects which common-ci-configuration revision the cache scripts
    # come from, so a pipeline can be run against a branch of that repo before it is
    # merged (these scripts are fetched at runtime, not pinned by common_includes.yml).
    # It is exported because copy_datadir.sh builds its own cache-manager.sh URL.
    COMMON_CI_REF="${COMMON_CI_REF:-develop}"
    COMMON_CI_URL="${COMMON_CI_URL:-https://gitlab.syncad.com/hive/common-ci-configuration/-/raw/${COMMON_CI_REF}}"
    export COMMON_CI_REF COMMON_CI_URL
    COPY_DATADIR_SCRIPT="/tmp/copy_datadir.sh"
    echo "Fetching copy_datadir.sh from common-ci-configuration..."
    wget -qO "$COPY_DATADIR_SCRIPT" "${COMMON_CI_URL}/haf-app-tools/scripts/copy_datadir.sh"
    chmod +x "$COPY_DATADIR_SCRIPT"
    source "$COPY_DATADIR_SCRIPT"
fi


if [[ ! -d "$DATADIR" ]]; then
    echo "Data directory (DATADIR) $DATADIR does not exist. Exiting."
    exit 1
fi

# Be sure this directory exists
mkdir --mode=775 -p "$DATADIR/blockchain"

if [[ ! -d "$SHM_DIR" ]]; then
    echo "Shared memory file directory (SHM_DIR) $SHM_DIR does not exist. Exiting."
    exit 1
fi

LOG_FILE="${DATADIR}/${LOG_FILE:=docker_entrypoint.log}"
touch "$LOG_FILE"
chmod a+rw "$LOG_FILE"

# shellcheck source=../scripts/common.sh
source "$SCRIPTSDIR/common.sh"

# shellcheck disable=SC2317
cleanup () {
  echo "Performing cleanup...."
  local hived_pid
  hived_pid=$(pidof 'hived' || echo '') # pidof returns 1 if hived isn't running, which crashes the script
  echo "Hived pid: $hived_pid"

  jobs -l

  [[ -z "$hived_pid" ]] || kill -INT "$hived_pid"

  echo "Waiting for hived finish..."
  [[ -z "$hived_pid" ]] || tail --pid="$hived_pid" -f /dev/null || true
  echo "Hived finish done."

  echo "Cleanup actions done."
}

######### Bootstrap modes #########
# By default one container brings the state up to date (downloads, snapshot load,
# replay) and then runs the node. An orchestrator that must not restart a refusal
# can split this in two containers on the same volumes:
#   HIVED_BOOTSTRAP_ONLY=1: bootstrap, run hived with --exit-before-sync and exit
#     with its exit code; exits 0 at once when a running hived holds the state.
#   HIVED_SKIP_BOOTSTRAP=1: run the node without downloading or loading anything.
BOOTSTRAP_ONLY="${HIVED_BOOTSTRAP_ONLY:-0}"
SKIP_BOOTSTRAP="${HIVED_SKIP_BOOTSTRAP:-0}"
if [[ "$BOOTSTRAP_ONLY" == "1" && "$SKIP_BOOTSTRAP" == "1" ]]; then
  echo "HIVED_BOOTSTRAP_ONLY and HIVED_SKIP_BOOTSTRAP cannot both be set. Exiting."
  exit 1
fi

# True when ARG names OPTION as hived reads it: the full name or an abbreviation
# (hived accepts any unambiguous prefix, and refuses to start on an ambiguous one),
# with or without "=value".
names_option() {
  local name="${1%%=*}" option="$2"
  [[ "$name" == --?* && "$option" == "$name"* ]]
}

# True when any of the hived arguments names one of the OPTIONs.
has_option() {
  local arg option
  for arg in "${HIVED_ARGS[@]}"; do
    for option in "$@"; do
      names_option "$arg" "$option" && return 0
    done
  done
  return 1
}

######### Block log bootstrap #########
BLOCKCHAIN_DIR="$DATADIR/blockchain"

# True when the blockchain directory already holds a block log: the monolithic
# file or at least one split part.
has_block_log() {
  [[ -f "$BLOCKCHAIN_DIR/block_log" ]] && return 0
  local part
  for part in "$BLOCKCHAIN_DIR"/block_log_part.*; do
    [[ -f "$part" ]] && return 0
  done
  return 1
}

# Resumable download of one file. The transfer lands in "<target>.partial" and is
# renamed only after wget succeeds, so a container restarted mid-download resumes
# instead of starting hived on a truncated file.
fetch_file() {
  local url="$1" target="$2"
  local partial="${target}.partial"
  echo "Downloading ${url} -> ${target}"
  wget --continue --tries=20 --waitretry=30 --read-timeout=60 --progress=dot:giga \
       --output-document="$partial" "$url" || return 1
  mv -f "$partial" "$target"
}

# Refuse to start a download that cannot fit. Skipped when the server sends no size.
check_free_space() {
  local url="$1" target="$2"
  local size have avail
  size=$(wget --spider --server-response --tries=3 "$url" 2>&1 \
         | sed -n 's/^ *Content-Length: *\([0-9]*\).*/\1/p' | tail -1) || true
  [[ -n "$size" ]] || return 0
  have=$(stat -c %s "${target}.partial" 2>/dev/null || echo 0)
  avail=$(df --block-size=1 --output=avail "$(dirname "$target")" | tail -1)
  if (( avail + have < size )); then
    echo "Not enough free space for ${url}: need $(numfmt --to=iec "$size"), have $(numfmt --to=iec "$((avail + have))"). Exiting."
    exit 1
  fi
}

# BLOCK_LOG_URL: URL of a monolithic block_log to fetch on first start, so a fresh
# volume bootstraps itself instead of syncing from genesis over P2P. Its .artifacts
# sidecar is fetched from BLOCK_LOG_ARTIFACTS_URL (default: BLOCK_LOG_URL + ".artifacts");
# when that is unavailable hived rebuilds it, which takes hours for a full block log.
# Nothing is downloaded when a block log is already present.
bootstrap_block_log() {
  [[ -n "${BLOCK_LOG_URL:-}" ]] || return 0
  if has_block_log; then
    echo "BLOCK_LOG_URL is set but ${BLOCKCHAIN_DIR} already holds a block log - not downloading."
    return 0
  fi
  local artifacts_url="${BLOCK_LOG_ARTIFACTS_URL:-${BLOCK_LOG_URL}.artifacts}"
  check_free_space "$BLOCK_LOG_URL" "$BLOCKCHAIN_DIR/block_log"
  fetch_file "$BLOCK_LOG_URL" "$BLOCKCHAIN_DIR/block_log"
  if ! fetch_file "$artifacts_url" "$BLOCKCHAIN_DIR/block_log.artifacts"; then
    rm -f "$BLOCKCHAIN_DIR/block_log.artifacts.partial"
    echo "WARNING: could not download ${artifacts_url}; hived will rebuild block_log.artifacts itself."
  fi
}

######### Snapshot bootstrap #########
# SNAPSHOT_URL: URL of a .tgz produced by hived's state_snapshot plugin, so a node
# comes up at the snapshot's block instead of replaying from genesis. Whenever an
# unpacked snapshot directory exists under $SNAPSHOT_DIR, hived gets
# --load-snapshot-if-no-state and decides by itself: it loads the snapshot only when
# there is no complete state (no state file, or a load that did not finish) and
# leaves a complete state alone. The entrypoint keeps no load state of its own; it
# downloads and unpacks the snapshot only when hived would load it, so the snapshot
# directory can be deleted once the node is up. Combined with BLOCK_LOG_URL the
# block log is kept and hived replays it forward from the snapshot's block by
# itself. Without a block log the node runs memory-only (block-log-split=0): it
# serves the chain state and account history, but not historical blocks, and it
# cannot replay later.
SNAPSHOT_DIR="$DATADIR/snapshot"
# Written by hived next to its state file while a snapshot load is unfinished:
# {"stage":"loading"|"replaying","snapshot":"<name>"}. Only "loading" needs the
# snapshot again; hived resumes a "replaying" one from the block log.
SNAPSHOT_LOAD_MARKER="$SHM_DIR/snapshot_load_pending.json"

# Name of the (single) unpacked snapshot directory under $SNAPSHOT_DIR, if any.
find_snapshot_name() {
  local entry
  for entry in "$SNAPSHOT_DIR"/*/; do
    [[ -d "$entry" ]] || continue
    basename "$entry"
    return 0
  done
  return 1
}

# True when hived would load a snapshot given --load-snapshot-if-no-state. hived
# takes a marker it cannot read for an unfinished load, so only "replaying" counts
# as a finished one here.
state_needs_snapshot() {
  [[ -f "$SHM_DIR/shared_memory.bin" ]] || return 0
  [[ -f "$SNAPSHOT_LOAD_MARKER" ]] || return 1
  ! grep -qF '"stage":"replaying"' "$SNAPSHOT_LOAD_MARKER"
}

# A node bootstrapped from a snapshot alone keeps no block log, whether or not this
# start bootstraps.
add_snapshot_node_args() {
  [[ -n "${SNAPSHOT_URL:-}" ]] || return 0
  has_block_log && return 0
  if has_option --block-log-split; then
    echo "A snapshot node without a block log needs block-log-split=0, which the entrypoint sets; drop --block-log-split from the arguments. Exiting."
    exit 1
  fi
  HIVED_ARGS+=("--block-log-split=0")
}

bootstrap_snapshot() {
  # Load record of an earlier entrypoint, replaced by hived's own marker.
  rm -f "$DATADIR/.snapshot_loaded"
  [[ -n "${SNAPSHOT_URL:-}" ]] || return 0
  # These rebuild or replace the state themselves, so no snapshot is added.
  if has_option --force-replay --resync-blockchain --load-snapshot --load-snapshot-if-no-state; then
    return 0
  fi
  local name=""
  name=$(find_snapshot_name) || true
  if [[ -z "$name" ]] && state_needs_snapshot; then
    local archive
    archive="$SNAPSHOT_DIR/$(basename "$SNAPSHOT_URL")"
    mkdir -p "$SNAPSHOT_DIR"
    check_free_space "$SNAPSHOT_URL" "$archive"
    fetch_file "$SNAPSHOT_URL" "$archive"
    echo "Unpacking $(basename "$archive") into ${SNAPSHOT_DIR}"
    tar --extract --gzip --file="$archive" --directory="$SNAPSHOT_DIR"
    rm -f "$archive"
    if ! name=$(find_snapshot_name); then
      echo "No snapshot directory found under ${SNAPSHOT_DIR} after unpacking ${SNAPSHOT_URL}. Exiting."
      exit 1
    fi
  fi
  [[ -n "$name" ]] || return 0
  echo "Passing snapshot ${name} to hived, which loads it only when there is no complete state (the ${SNAPSHOT_DIR} directory can be deleted once the node is up)."
  HIVED_ARGS+=("--load-snapshot-if-no-state=$name")
}

######### Replay detection #########
# hived refuses to start when its state lags the block log (a downloaded or copied-in
# block log, or a crash between state flushes) unless --replay-blockchain is given,
# yet aborts on --replay-blockchain when there is no block log at all. With state up
# to date the flag is a no-op resume. HIVED_AUTO_REPLAY=1 adds it whenever a block log
# exists and the caller did not pick a replay mode. Off by default: a state/block log
# mismatch then stops hived with an error instead of silently starting a replay. The
# options are also recognised by the abbreviations hived accepts, such as --replay.
# --load-snapshot-if-no-state is not --load-snapshot: it allows a replay.
maybe_add_replay_arg() {
  [[ "${HIVED_AUTO_REPLAY:-0}" == "1" ]] || return 0
  if has_option --replay-blockchain --force-replay --resync-blockchain --load-snapshot; then
    return 0
  fi
  if has_block_log; then
    echo "Block log present: HIVED_AUTO_REPLAY=1, adding --replay-blockchain."
    HIVED_ARGS+=("--replay-blockchain")
  fi
}

# True when a running hived holds the state: shared_memory_file_util is refused
# the lock that hived keeps for as long as it runs.
state_in_use() {
  [[ -f "$SHM_DIR/shared_memory.bin" ]] || return 1
  local output
  output=$(/home/hived/bin/shared_memory_file_util --input "$SHM_DIR" --list-plugins 2>&1) && return 1
  grep -qF "in use by another process" <<< "$output"
}

HIVED_ARGS=()
HIVED_ARGS+=("$@")
export HIVED_ARGS

if [[ "$BOOTSTRAP_ONLY" == "1" ]] && state_in_use; then
  echo "HIVED_BOOTSTRAP_ONLY=1: a running hived holds the state, so there is nothing to bootstrap."
  exit 0
fi

if [[ "$SKIP_BOOTSTRAP" != "1" ]]; then
  bootstrap_block_log
fi
add_snapshot_node_args
if [[ "$SKIP_BOOTSTRAP" != "1" ]]; then
  bootstrap_snapshot
fi
maybe_add_replay_arg
if [[ "$BOOTSTRAP_ONLY" == "1" ]] && ! has_option --exit-before-sync; then
  HIVED_ARGS+=("--exit-before-sync")
fi

run_instance() {
trap cleanup INT TERM
trap cleanup EXIT

echo "Attempting to execute hived using additional command line arguments: ${HIVED_ARGS[*]}"

# Listening endpoints for hived, overridable via the container environment so a
# node can opt into IPv6, e.g. P2P_ENDPOINT=[::]:2001 (dual-stack on Linux) or a
# specific address such as P2P_ENDPOINT=[2001:db8::1]:2001.
# Defaults stay 0.0.0.0 (IPv4) for now: switching the default to dual-stack [::]
# is deferred until a release includes the P2P outbound-connection bind fix, so
# that current binaries keep sourcing outbound connections from the listening port.
P2P_ENDPOINT="${P2P_ENDPOINT:-0.0.0.0:${P2P_PORT}}"
WS_ENDPOINT="${WS_ENDPOINT:-0.0.0.0:${WS_PORT}}"
HTTP_ENDPOINT="${HTTP_ENDPOINT:-0.0.0.0:${HTTP_PORT}}"

# The arguments are passed as they are, never re-parsed by a shell: a snapshot name
# or an option value may hold spaces or shell syntax. hived's exit code is passed on
# explicitly: run_instance runs in a `||` list, where set -e has no effect.
{
/home/hived/bin/hived --webserver-ws-endpoint="${WS_ENDPOINT}" --webserver-http-endpoint="${HTTP_ENDPOINT}" --p2p-endpoint="${P2P_ENDPOINT}" \
  --data-dir="$DATADIR" --shared-file-dir="$SHM_DIR" \
  "${HIVED_ARGS[@]}" 2>&1 | tee -i "$DATADIR/hived.log"
hived_return_code=${PIPESTATUS[0]}
echo "$hived_return_code Hived process finished execution."
exit "$hived_return_code"
} &

job_pid=$!

jobs -l

echo "waiting for job finish: $job_pid."
local status=0
wait $job_pid || status=$?

if [ $status -eq 130 ];
then
  echo "Ignoring SIGINT exit code: $status."
  status=0 #ignore exitcode caught by handling SIGINT
elif [ $status -eq 78 ];
then
  echo "hived refused to start for a reason a restart cannot fix (exit code 78); see the log above."
fi

echo "Hived process finished execution: return status: ${status}."

return ${status}
}

status=0
run_instance || status=$?

echo "Exiting docker entrypoint with status: ${status}..."
exit $status

#! /bin/bash

set -euo pipefail

# Optional UID override: if HIVED_UID is set, remap the hived user to that UID.
# This is only needed when bind-mounting volumes owned by a non-1000 UID.
if [[ -n "${HIVED_UID:-}" ]] && [[ "${HIVED_UID}" =~ ^[0-9]+$ ]] && [[ "${HIVED_UID}" -ne 0 ]] && [[ "${HIVED_UID}" -ne "$(id -u)" ]]; then
  echo "Remapping hived UID to ${HIVED_UID}"
  sudo -n usermod -o -u "${HIVED_UID}" hived
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

######### Replay detection #########
# hived refuses to start when its state lags the block log (a downloaded or copied-in
# block log, or a crash between state flushes) unless --replay-blockchain is given,
# yet aborts on --replay-blockchain when there is no block log at all. With state up
# to date the flag is a no-op resume, so add it whenever a block log exists and the
# caller did not pick a replay mode. HIVED_AUTO_REPLAY=0 turns this off.
maybe_add_replay_arg() {
  [[ "${HIVED_AUTO_REPLAY:-1}" == "1" ]] || return 0
  local arg
  for arg in "${HIVED_ARGS[@]}"; do
    case "$arg" in
      --replay-blockchain*|--force-replay*|--resync-blockchain*|--load-snapshot*)
        return 0 ;;
    esac
  done
  if has_block_log; then
    echo "Block log present: adding --replay-blockchain (set HIVED_AUTO_REPLAY=0 to disable)."
    HIVED_ARGS+=("--replay-blockchain")
  fi
}

HIVED_ARGS=()
HIVED_ARGS+=("$@")
export HIVED_ARGS

bootstrap_block_log
maybe_add_replay_arg

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

{
/bin/bash << EOF
echo "Attempting to execute hived using additional command line arguments: ${HIVED_ARGS[*]}"
set -euo pipefail

/home/hived/bin/hived --webserver-ws-endpoint="${WS_ENDPOINT}" --webserver-http-endpoint="${HTTP_ENDPOINT}" --p2p-endpoint="${P2P_ENDPOINT}" \
  --data-dir="$DATADIR" --shared-file-dir="$SHM_DIR"  \
  ${HIVED_ARGS[@]} 2>&1 | tee -i "$DATADIR/hived.log"
hived_return_code="\$?"
echo "\$hived_return_code Hived process finished execution."
EOF

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
fi

echo "Hived process finished execution: return status: ${status}."

return ${status}
}

run_instance
status=$?

echo "Exiting docker entrypoint with status: ${status}..."
exit $status

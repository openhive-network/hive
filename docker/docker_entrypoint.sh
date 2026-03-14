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
    COMMON_CI_URL="${COMMON_CI_URL:-https://gitlab.syncad.com/hive/common-ci-configuration/-/raw/develop}"
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

HIVED_ARGS=()
HIVED_ARGS+=("$@")
export HIVED_ARGS

run_instance() {
trap cleanup INT TERM
trap cleanup EXIT

echo "Attempting to execute hived using additional command line arguments: ${HIVED_ARGS[*]}"

{
/bin/bash << EOF
echo "Attempting to execute hived using additional command line arguments: ${HIVED_ARGS[*]}"
set -euo pipefail

/home/hived/bin/hived --webserver-ws-endpoint=0.0.0.0:${WS_PORT} --webserver-http-endpoint=0.0.0.0:${HTTP_PORT} --p2p-endpoint=0.0.0.0:${P2P_PORT} \
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

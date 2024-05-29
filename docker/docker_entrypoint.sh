#! /bin/bash

set -euo pipefail

if [[ -z "${HIVED_UID:-}" ]]; then
  echo "Warning: variable HIVED_UID is not set or set to an empty value." >&2
elif ! [[ "${HIVED_UID}" =~ ^[0-9]+$ ]] ; then
  echo "Error: variable HIVED_UID is set to '${HIVED_UID}' and not an integer. Exiting..." >&2
  exit 1
elif [[ "${HIVED_UID}" -ne 0 ]];
then
  echo "Setting user hived's UID to value '${HIVED_UID}'"
  sudo -n usermod -o -u "${HIVED_UID}" hived
fi


SCRIPTDIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTDIR/scripts"

"$SCRIPTSDIR/copy_datadir.sh"


if sudo -Enu hived test ! -d "$DATADIR"
then
    echo "Data directory (DATADIR) $DATADIR does not exist. Exiting."
    exit 1
fi

# Be sure this directory exists
sudo -Enu hived mkdir --mode=775 -p "$DATADIR/blockchain"

if sudo -Enu hived test ! -d "$SHM_DIR"
then
    echo "Shared memory file directory (SHM_DIR) $SHM_DIR does not exist. Exiting."
    exit 1
fi

LOG_FILE="${DATADIR}/${LOG_FILE:=docker_entrypoint.log}"
sudo -n touch "$LOG_FILE"
sudo -n chown -Rc hived:users "$LOG_FILE"
sudo -n chmod a+rw "$LOG_FILE"

# shellcheck source=../scripts/common.sh
source "$SCRIPTSDIR/common.sh"

# shellcheck disable=SC2317
cleanup () {
  echo "Performing cleanup...."
  local hived_pid
  hived_pid=$(pidof 'hived' || echo '') # pidof returns 1 if hived isn't running, which crashes the script
  echo "Hived pid: $hived_pid"

  jobs -l

  [[ -z "$hived_pid" ]] || sudo -n kill -INT "$hived_pid"

  echo "Waiting for hived finish..."
  [[ -z "$hived_pid" ]] || tail --pid="$hived_pid" -f /dev/null || true
  echo "Hived finish done."

  echo "Cleanup actions done."
}

HIVED_ARGS=()
HIVED_ARGS+=("$@")
export HIVED_ARGS

run_instance() {
# What can be a difference to catch EXIT instead of SIGINT ? Found here: https://gist.github.com/CMCDragonkai/e2cde09b688170fb84268cafe7a2b509
#trap 'exit' INT QUIT TERM
#trap cleanup EXIT
trap cleanup INT TERM
trap cleanup EXIT

echo "Attempting to execute hived using additional command line arguments: ${HIVED_ARGS[*]}"

{
sudo -Enu hived /bin/bash << EOF
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
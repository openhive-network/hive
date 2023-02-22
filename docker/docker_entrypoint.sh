#! /bin/bash

set -xeuo pipefail

env

if [ "$(id -u)" != 0 ] && [ -n "${NEW_UID+x}" ];
then
  sudo -En "$0" "$@"
  exit $?
fi

if [ "$(id -u)" = 0 ] && [ -n "${NEW_UID+x}" ];
then
  sudo -n usermod -o -u "${NEW_UID}" hived
  unset NEW_UID
  sudo -Enu hived "$0" "$@"
  exit $?
fi


SCRIPTDIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTDIR/scripts"

if [ ! -d "$DATADIR" ];
then
    echo "Data directory (DATADIR) $DATADIR does not exist. Exiting."
    exit 1
fi

if [ ! -d "$SHM_DIR" ];
then
    echo "Shared memory file directory (SHM_DIR) $SHM_DIR does not exist. Exiting."
    exit 1
fi


LOG_FILE="${DATADIR}/${LOG_FILE:=docker_entrypoint.log}"
sudo -n touch "$LOG_FILE"
sudo -n chown -Rc hived:hived "$LOG_FILE"
sudo -n chmod a+rw "$LOG_FILE"
source "$SCRIPTSDIR/common.sh"


cleanup () {
  echo "Performing cleanup...."
  hived_pid=$(pidof 'hived')
  echo "Hived pid: $hived_pid"
  
  sudo -n kill -INT $hived_pid

  echo "Waiting for hived finish..."
  tail --pid=$hived_pid -f /dev/null || true
  echo "Hived finish done."

  echo "Cleanup actions done."
}

# What can be a difference to catch EXIT instead of SIGINT ? Found here: https://gist.github.com/CMCDragonkai/e2cde09b688170fb84268cafe7a2b509
#trap 'exit' INT QUIT TERM
#trap cleanup EXIT
trap cleanup INT QUIT TERM

sudo -n chown -Rc hived:hived "$DATADIR"
sudo -n chown -Rc hived:hived "$SHM_DIR"

# Be sure this directory exists
sudo -Enu hived mkdir --mode=777 -p "$DATADIR/blockchain"

cd "$DATADIR"

HIVED_ARGS=()
HIVED_ARGS+=("$@")
export HIVED_ARGS

echo "Attempting to execute hived using additional command line arguments: ${HIVED_ARGS[@]}"

{
sudo -Enu hived /bin/bash << EOF
echo "Attempting to execute hived using additional command line arguments: ${HIVED_ARGS[@]}"

/home/hived/bin/hived --webserver-ws-endpoint=0.0.0.0:${WS_PORT} --webserver-http-endpoint=0.0.0.0:${HTTP_PORT} --p2p-endpoint=0.0.0.0:${P2P_PORT} \
  --data-dir="$DATADIR" --shared-file-dir="$SHM_DIR"  \
  ${HIVED_ARGS[@]} 2>&1 | tee -i hived.log
echo "$? Hived process finished execution."
EOF

} &

job_pid=$!

jobs -l

echo "waiting for job finish: $job_pid."
wait $job_pid || true

echo "Exiting docker entrypoint..."


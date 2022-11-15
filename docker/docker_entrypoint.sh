#! /bin/bash

set -xeuo pipefail

# this is only for demonstration purposes and is waiting for unpacking cache when using cache keys
sleep 360

SCRIPTDIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTDIR/scripts"

LOG_FILE=docker_entrypoint.log
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

if [ ! -d /home/hived/datadir ]
then
  sudo -n mkdir -p /home/hived/datadir
fi

if [ -z ${DATADIR-} ]
then
  DATADIR=/home/hived/datadir
fi

if [ -z ${SHM_DIR-} ]
then
  SHM_DIR=/home/hived/shm_dir
fi

RED='\033[0;31m'
NC='\033[0m' # No Color
if [ ! -d "$DATADIR" ];
then
    echo "${RED}Data directory (DATADIR) '$DATADIR' does not exist. Exiting.${NC}"
    exit 1
fi

if [ ! -d "$SHM_DIR" ];
then
    echo "${RED}Shared memory file directory (SHM_DIR) '$SHM_DIR' does not exist. Exiting.${NC}"
    exit 1
fi


sudo -n chown -Rc hived:hived /home/hived/datadir

# Be sure this directory exists
mkdir --mode=777 -p /home/hived/datadir/blockchain
sudo -n chown -Rc hived:hived /home/hived/shm_dir

cd $DATADIR

HIVED_ARGS=()
HIVED_ARGS+=("$@")
export HIVED_ARGS

echo "Attempting to execute hived using additional command line arguments: ${HIVED_ARGS[@]}"

export FAKETIME_TIMESTAMP_FILE=$DATADIR/faketime.rc

{
sudo -Enu hived /bin/bash << EOF
echo "Attempting to execute hived using additional command line arguments: ${HIVED_ARGS[@]}"

LD_PRELOAD=/home/hived/hive_base_config/libfaketime/src/libfaketimeMT.so.1 \
/home/hived/bin/hived --webserver-ws-endpoint=0.0.0.0:${WS_PORT} --webserver-http-endpoint=0.0.0.0:${HTTP_PORT} --p2p-endpoint=0.0.0.0:${P2P_PORT} \
  --data-dir=${DATADIR} --shared-file-dir=${SHM_DIR} \
  ${HIVED_ARGS[@]} 2>&1 | tee -i hived.log
echo "$? Hived process finished execution."
EOF

} &

job_pid=$!

jobs -l

echo "waiting for job finish: $job_pid."
wait $job_pid || true

echo "Exiting docker entrypoint..."


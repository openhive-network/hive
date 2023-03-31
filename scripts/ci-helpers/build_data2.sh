#! /bin/bash
set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."

LOG_FILE=build_data.log
source "$SCRIPTSDIR/common.sh"


IMG=""
DATA_CACHE=""
CONFIG_INI_SOURCE=""
BLOCK_LOG_SOURCE_DIR=""

print_help () {
    echo "Usage: $0 <image> [OPTION[=VALUE]]..."
    echo
    echo "Allows to build docker image containing Hived installation"
    echo "OPTIONS:"
    echo "  --data-cache=PATH             Allows to specify a directory where data and shared memory file should be stored."
    echo "  --block-log-source-dir=PATH   Allows to specify a directory of block_log used to perform initial replay."
    echo "  --config-ini-source=PATH      Allows to specify a path of config.ini configuration file used for building data."
    echo "  --help                        Display this help screen and exit"
    echo
}

while [ $# -gt 0 ]; do
  case "$1" in
    --data-cache=*)
        DATA_CACHE="${1#*=}"
        echo "using DATA_CACHE $DATA_CACHE"
        ;;
    --block-log-source-dir=*)
        BLOCK_LOG_SOURCE_DIR="${1#*=}"
        echo "using BLOCK_LOG_SOURCE_DIR $BLOCK_LOG_SOURCE_DIR"
        ;;
    --config-ini-source=*)
        CONFIG_INI_SOURCE="${1#*=}"
        echo "using CONFIG_INI_SOURCE $CONFIG_INI_SOURCE"
        ;;
    --help)
        print_help
        exit 0
        ;;
    *)
        if [ -z "$IMG" ];
        then
          IMG="$1"
        else
          echo "ERROR: '$1' is not a valid option/positional argument"
          echo
          print_help
          exit 2
        fi
        ;;
    esac
    shift
done

if [ -f "$DATA_CACHE/datadir/status" ];
then
    echo "Previous replay exit code"
    status=$(cat "$DATA_CACHE/datadir/status")
    echo "$status"
    if [ "$status" -eq 0 ];
    then
        echo "Previous replay datadir is valid, exiting"
        exit 0
    fi
fi
echo "Didnt find valid previous replay, performing fresh replay"
ls "$DATA_CACHE" -lath
rm "$DATA_CACHE/datadir" -rf
rm "$DATA_CACHE/shm_dir" -rf

echo "Preparing datadir and shm_dir in location ${DATA_CACHE}"
"$SCRIPTPATH/prepare_data_and_shm_dir.sh" --data-base-dir="$DATA_CACHE" \
    --block-log-source-dir="$BLOCK_LOG_SOURCE_DIR" --config-ini-source="$CONFIG_INI_SOURCE"

echo "Attempting to perform replay basing on image ${IMG}..."

"$SCRIPTSDIR/run_hived_img.sh" --name=hived_instance \
    --detach \
    --docker-option=--volume="$DATA_CACHE":"$DATA_CACHE" \
    --docker-option=--env=DATADIR="$DATA_CACHE/datadir" \
    --docker-option=--env=SHM_DIR="$DATA_CACHE/shm_dir" \
    --docker-option=--env=HIVED_UID="$(id -u)" \
    $IMG --replay-blockchain --stop-replay-at-block=5000000 --exit-before-sync

echo "Logs from container hived_instance:"
docker logs -f hived_instance &

status=$(docker wait hived_instance)

echo "$status" > "$DATA_CACHE/datadir/status"
exit $status

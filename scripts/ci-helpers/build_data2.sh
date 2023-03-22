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
FAKETIME_TIMESTAMP=""

print_help () {
    echo "Usage: $0 <image> [OPTION[=VALUE]]..."
    echo
    echo "Allows to build docker image containing Hived installation"
    echo "OPTIONS:"
    echo "  --data-cache=PATH             Allows to specify a directory where data and shared memory file should be stored."
    echo "  --block-log-source-dir=PATH   Allows to specify a directory of block_log used to perform initial replay."
    echo "  --config-ini-source=PATH      Allows to specify a path of config.ini configuration file used for building data."
    echo "  --faketime-timestamp=FMT      Faketime in ISO 8601 extended format as in block api. Example: 2016-09-15T19:47:21"
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
    --faketime-timestamp=*)
        FAKETIME_TIMESTAMP="${1#*=}"
        echo "using FAKETIME_TIMESTAMP $FAKETIME_TIMESTAMP"
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


echo "Preparing datadir and shm_dir in location ${DATA_CACHE}"
rm "$DATA_CACHE" -rf
"$SCRIPTPATH/prepare_data_and_shm_dir.sh" --data-base-dir="$DATA_CACHE" \
    --block-log-source-dir="$BLOCK_LOG_SOURCE_DIR" --config-ini-source="$CONFIG_INI_SOURCE" \
    --faketime-timestamp="$FAKETIME_TIMESTAMP"

echo "Attempting to perform replay basing on image ${IMG}..."

"$SCRIPTSDIR/run_hived_img.sh" --name=hived_instance \
    --detach \
    --docker-option=--volume="$DATA_CACHE":"$DATA_CACHE" \
    --docker-option=--env=DATADIR="$DATA_CACHE/datadir" \
    --docker-option=--env=SHM_DIR="$DATA_CACHE/shm_dir" \
    $IMG --replay-blockchain --stop-replay-at-block=5000000 --exit-before-sync

echo "Logs from container hived_instance:"
docker logs -f hived_instance &

exit $(docker wait hived_instance)

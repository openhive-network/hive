#! /bin/bash
set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."

LOG_FILE=build_data.log
source "$SCRIPTSDIR/common.sh"


DATA_BASE_DIR=""
CONFIG_INI_SOURCE=""
BLOCK_LOG_SOURCE_DIR=""

print_help () {
    echo "Usage: $0 [OPTION[=VALUE]]..."
    echo
    echo "Allows to build docker image containing Hived installation"
    echo "OPTIONS:"
    echo "  --data-base-dir=PATH        Allows to specify a directory where data and shared memory file should be stored."
    echo "  --block-log-source-dir=PATH Allows to specify a directory of block_log used to perform initial replay."
    echo "  --config-ini-source=PATH    Allows to specify a path of config.ini configuration file used for building data."
    echo "  --help                      Display this help screen and exit"
    echo
}

while [ $# -gt 0 ]; do
  case "$1" in
    --data-base-dir=*)
        DATA_BASE_DIR="${1#*=}"
        echo "using DATA_BASE_DIR $DATA_BASE_DIR"
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
        echo "ERROR: '$1' is not a valid option/positional argument"
        echo
        print_help
        exit 2
        ;;
    esac
    shift
done


echo "Attempting to perform 5000000 block replay basing on ${HIVED_IMAGE_NAME}..."


"$SCRIPTPATH/prepare_data_and_shm_dir.sh" --data-base-dir="$DATA_BASE_DIR" \
    --block-log-source-dir="$BLOCK_LOG_SOURCE_DIR" --config-ini-source="$CONFIG_INI_SOURCE"

"$SCRIPTSDIR/run_hived_img.sh" --name=hived_instance \
    --shared-file-dir=$DATA_BASE_DIR/shm_dir --data-dir=$DATA_BASE_DIR/datadir \
    $HIVED_IMAGE_NAME --replay-blockchain --stop-replay-at-block=5000000 --exit-before-sync

docker logs -f hived_instance
exit $(docker wait hived_instance)

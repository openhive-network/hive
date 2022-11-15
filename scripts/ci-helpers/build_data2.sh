#! /bin/bash
set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."

LOG_FILE=build_data.log
source "$SCRIPTSDIR/common.sh"

BUILD_IMAGE_TAG=""
SRCROOTDIR=""
REGISTRY=""
IMAGE_TAG_PREFIX=""

NETWORK_TYPE_ARG=""
EXPORT_BINARIES_ARG=""

DATA_BASE_DIR=""
CONFIG_INI_SOURCE=""
BLOCK_LOG_SOURCE_DIR=""
FAKETIME_TIMESTAMP=""

print_help () {
    echo "Usage: $0 <image_tag> <src_dir> <registry_url> <data_and_shm_base_dir> [OPTION[=VALUE]]..."
    echo
    echo "Allows to build docker image containing Hived installation"
    echo "OPTIONS:"
    echo "  --network-type=TYPE         Allows to specify type of blockchain network supported by built hived. Allowed values: mainnet, testnet, mirrornet"
    echo "  --export-binaries=PATH      Allows to specify a path where binaries shall be exported from built image."
    echo "  --data-base-dir=PATH        Allows to specify a directory where data and shared memory file should be stored."
    echo "  --block-log-source-dir=PATH Allows to specify a directory of block_log used to perform initial replay."
    echo "  --config-ini-source=PATH    Allows to specify a path of config.ini configuration file used for building data."
    echo "  --faketime-timestamp=FMT    Timestamp (for example 5000000) in iso 8601 format"
    echo "  --help                      Display this help screen and exit"
    echo
}

while [ $# -gt 0 ]; do
  case "$1" in
    --network-type=*)
        type="${1#*=}"
        NETWORK_TYPE_ARG="--network-type=${type}"

        case $type in
          "testnet"*)
            IMAGE_TAG_PREFIX=testnet-
            ;;
          "mirrornet"*)
            IMAGE_TAG_PREFIX=mirrornet-
            ;;
          "mainnet"*)
            IMAGE_TAG_PREFIX=
            ;;
           *)
            echo "ERROR: '$type' is not a valid network type"
            echo
            exit 3
        esac
        ;;
    --export-binaries=*)
        export_path="${1#*=}"
        EXPORT_BINARIES_ARG="--export-binaries=${export_path}"
        ;;
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
    --faketime-timestamp=*)
        FAKETIME_TIMESTAMP="${1#*=}"
        echo "using FAKETIME_TIMESTAMP $FAKETIME_TIMESTAMP"
        ;;
    --help)
        print_help
        exit 0
        ;;
    *)
        if [ -z "$BUILD_IMAGE_TAG" ];
        then
          BUILD_IMAGE_TAG="${1}"
        elif [ -z "$SRCROOTDIR" ];
        then
          SRCROOTDIR="${1}"
        elif [ -z "$REGISTRY" ];
        then
          REGISTRY=${1}
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

# Supplement a registry path by trailing slash (if needed)
[[ "${REGISTRY}" != */ ]] && REGISTRY="${REGISTRY}/"

"$SCRIPTSDIR/ci-helpers/build_instance.sh" "${BUILD_IMAGE_TAG}" "${SRCROOTDIR}" "${REGISTRY}" "${NETWORK_TYPE_ARG}" "${EXPORT_BINARIES_ARG}"

echo "Instance image built. Attempting to perform 5000000 block replay basing on it..."
HIVED_IMAGE_NAME=${REGISTRY}${IMAGE_TAG_PREFIX}instance:${IMAGE_TAG_PREFIX}instance-${BUILD_IMAGE_TAG}

time docker push $HIVED_IMAGE_NAME


"$SCRIPTPATH/prepare_data_and_shm_dir.sh" --data-base-dir="$DATA_BASE_DIR" \
    --block-log-source-dir="$BLOCK_LOG_SOURCE_DIR" --faketime-timestamp="$FAKETIME_TIMESTAMP" \
    --config-ini-source="$CONFIG_INI_SOURCE"

"$SCRIPTSDIR/run_hived_img.sh" --name=hived_instance \
    --shared-file-dir=$DATA_BASE_DIR/shm_dir --data-dir=$DATA_BASE_DIR/datadir \
    $HIVED_IMAGE_NAME --replay-blockchain --stop-replay-at-block=5000000 --exit-before-sync

docker logs -f hived_instance
exit $(docker wait hived_instance)

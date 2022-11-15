#! /bin/bash

set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."
SRCROOTDIR="$SCRIPTSDIR/.."

source "$SCRIPTPATH/docker_image_utils.sh"


submodule_path=""
REGISTRY=""
DOTENV_VAR_NAME=""
REGISTRY_USER=""
REGISTRY_PASSWORD=""

NETWORK_TYPE=""
EXPORT_BINARIES=""

DATA_BASE_DIR=""
CONFIG_INI_SOURCE=""
BLOCK_LOG_SOURCE_DIR=""
FAKETIME_TIMESTAMP=""

print_help () {
    echo "Usage: $0 <submodule_path> <registry> <dotenv_var_name> <registry_user> <registry_password> [OPTION[=VALUE]]..."
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


IMGNAME=instance

while [ $# -gt 0 ]; do
  case "$1" in
    --network-type=*)
        NETWORK_TYPE="${1#*=}"
        echo "using NETWORK_TYPE $NETWORK_TYPE"

        case $NETWORK_TYPE in
          "testnet"*)
            IMGNAME=testnet-instance
            ;;
          "mirrornet"*)
            IMGNAME=mirrornet-instance
            ;;
          "mainnet"*)
            ;;
           *)
            echo "ERROR: '$NETWORK_TYPE' is not a valid network type"
            echo
            exit 3
        esac
        ;;
    --export-binaries=*)
        EXPORT_BINARIES="${1#*=}"
        echo "using EXPORT_BINARIES $EXPORT_BINARIES"
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
        if [ -z "$submodule_path" ];
        then
          submodule_path="${1}"
        elif [ -z "$REGISTRY" ];
        then
          REGISTRY="${1}"
        elif [ -z "$DOTENV_VAR_NAME" ];
        then
          DOTENV_VAR_NAME=${1}
        elif [ -z "$REGISTRY_USER" ];
        then
          REGISTRY_USER=${1}
        elif [ -z "$REGISTRY_PASSWORD" ];
        then
          REGISTRY_PASSWORD=${1}
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

retrieve_submodule_commit () {
  local p="${1}"
  pushd "$p" >/dev/null 2>&1
  local commit=$( git rev-parse HEAD )

  popd >/dev/null 2>&1

  echo "$commit"
}

echo "Attempting to get commit for: $submodule_path"

commit=$( retrieve_submodule_commit ${submodule_path} )

img=$( build_image_name $IMGNAME $commit $REGISTRY )
img_path=$( build_image_registry_path $IMGNAME $commit $REGISTRY )
img_tag=$( build_image_registry_tag $IMGNAME $commit $REGISTRY )

echo $REGISTRY_PASSWORD | docker login -u $REGISTRY_USER $REGISTRY --password-stdin

image_exists=0

docker_image_exists $IMGNAME $commit $REGISTRY image_exists

if [ "$image_exists" -eq 1 ];
then
  echo "Image already exists..."
  "$SCRIPTPATH/export-binaries.sh" ${img} "${BINARY_CACHE_PATH}"

  if [ -z "$(ls -A $DATA_BASE_DIR/shm_dir)" ]; then
    echo "Shared memory file not found in $DATA_BASE_DIR/shm_dir, performing replay"

    "$SCRIPTPATH/prepare_data_and_shm_dir.sh" --data-base-dir="$DATA_BASE_DIR" \
        --block-log-source-dir="$BLOCK_LOG_SOURCE_DIR" --faketime-timestamp="$FAKETIME_TIMESTAMP" \
        --config-ini-source="$CONFIG_INI_SOURCE"
      
    "$SCRIPTSDIR/run_hived_img.sh" --name=hived_instance \
        --shared-file-dir=$DATA_BASE_DIR/shm_dir --data-dir=$DATA_BASE_DIR/datadir \
        $img --replay-blockchain --stop-replay-at-block=5000000 --exit-before-sync

    docker logs -f hived_instance
    docker wait hived_instance
  fi
else
  # Here continue an image build.
  echo "${img} image is missing. Building it..."

  "$SCRIPTPATH/build_data4commit2.sh" $commit $REGISTRY --export-binaries="${BINARY_CACHE_PATH}" --network-type=$NETWORK_TYPE \
    --data-base-dir="$DATA_BASE_DIR" \
    --block-log-source-dir="$BLOCK_LOG_SOURCE_DIR" --faketime-timestamp="$FAKETIME_TIMESTAMP" \
    --config-ini-source="$CONFIG_INI_SOURCE"
  time docker push $img
fi

echo "$DOTENV_VAR_NAME=$img" > docker_image_name.env
echo "${DOTENV_VAR_NAME}_REGISTRY_PATH=$img_path" >> docker_image_name.env
echo "${DOTENV_VAR_NAME}_REGISTRY_TAG=$img_tag" >> docker_image_name.env

cat docker_image_name.env

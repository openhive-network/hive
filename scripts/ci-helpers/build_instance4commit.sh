#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."
SRCROOTDIR="$SCRIPTSDIR/.."

LOG_FILE=build_instance4commit.log
source "$SCRIPTSDIR/common.sh"

COMMIT=""

REGISTRY=""

IMAGE_TAG_PREFIX=""

CI_IMAGE_TAG=:ubuntu20.04-3

BUILD_HIVE_TESTNET=OFF
HIVE_CONVERTER_BUILD=OFF


print_help () {
    echo "Usage: $0 <commit> [<registry_url>] [OPTION[=VALUE]]..."
    echo
    echo "Allows to setup this machine for Hived installation"
    echo "OPTIONS:"
    echo "  --network-type=TYPE       Allows to specify type of blockchain network supported by built hived. Allowed values: mainnet, testnet, mirrornet"
    echo "  --help                    Display this help screen and exit"
    echo
}

while [ $# -gt 0 ]; do
  case "$1" in
    --network-type=*)
        type="${1#*=}"

        case $type in
          "testnet"*)
            BUILD_HIVE_TESTNET=ON
            IMAGE_TAG_PREFIX=testnet-
            ;;
          "mirrornet"*)
            BUILD_HIVE_TESTNET=OFF
            HIVE_CONVERTER_BUILD=ON
            IMAGE_TAG_PREFIX=mirror-
            ;;
          "mainnet"*)
            BUILD_HIVE_TESTNET=OFF
            HIVE_CONVERTER_BUILD=OFF
            IMAGE_TAG_PREFIX=
            ;;
           *)
            echo "ERROR: '$type' is not a valid network type"
            echo
            exit 3
        esac
        ;;
    -*)
        echo "ERROR: '$1' is not a valid option"
        exit 1
        ;;
    *)
        if [ -z "$COMMIT" ];
        then
          COMMIT="$1"
        elif [ -z "$REGISTRY" ];
        then
          REGISTRY=$1
        else
          echo "ERROR: '$1' is not a valid positional argument"
          echo
          print_help
          exit 2
        fi
        ;;
    esac
    shift
done

if [ -z "$REGISTRY" ];
then
  echo "Setting default registry value"
  REGISTRY=registry.gitlab.syncad.com/hive/hive/
fi

BUILD_IMAGE_TAG=:$COMMIT

pushd "$SRCROOTDIR"
pwd

docker build --target=base_instance \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  --build-arg BUILD_HIVE_TESTNET=$BUILD_HIVE_TESTNET \
  --build-arg HIVE_CONVERTER_BUILD=$HIVE_CONVERTER_BUILD \
  --build-arg BLOCK_LOG_SUFFIX=${BLOCK_LOG_SUFFIX:-""} \
  --build-arg COMMIT=$COMMIT --build-arg BUILD_IMAGE_TAG=$BUILD_IMAGE_TAG -t ${REGISTRY}${IMAGE_TAG_PREFIX}base_instance${BLOCK_LOG_SUFFIX:-""}$BUILD_IMAGE_TAG -f Dockerfile .

docker build --target=instance \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  --build-arg BUILD_HIVE_TESTNET=$BUILD_HIVE_TESTNET \
  --build-arg HIVE_CONVERTER_BUILD=$HIVE_CONVERTER_BUILD \
  --build-arg BLOCK_LOG_SUFFIX=${BLOCK_LOG_SUFFIX:-""} \
  --build-arg COMMIT=$COMMIT --build-arg BUILD_IMAGE_TAG=$BUILD_IMAGE_TAG -t ${REGISTRY}${IMAGE_TAG_PREFIX}instance${BLOCK_LOG_SUFFIX:-""}$BUILD_IMAGE_TAG -f Dockerfile .

popd



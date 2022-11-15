#! /bin/bash

set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."
SRCROOTDIR="$SCRIPTSDIR/.."

LOG_FILE=build_instance4commit.log
source "$SCRIPTSDIR/common.sh"

COMMIT=""

REGISTRY=""

BRANCH="master"

NETWORK_TYPE_ARG=""

EXPORT_BINARIES_ARG=""

print_help () {
    echo "Usage: $0 <commit> <registry_url> [OPTION[=VALUE]]..."
    echo
    echo "Allows to build docker image containing Hived installation built from pointed COMMIT"
    echo "OPTIONS:"
    echo "  --network-type=TYPE       Allows to specify type of blockchain network supported by built hived. Allowed values: mainnet, testnet, mirrornet"
    echo "  --help                    Display this help screen and exit"
    echo
}

while [ $# -gt 0 ]; do
  case "$1" in
    --network-type=*)
        type="${1#*=}"
        NETWORK_TYPE_ARG="--network-type=${type}"
        echo "Using NETWORK_TYPE_ARG $NETWORK_TYPE_ARG"
        ;;
    --export-binaries=*)
        export_path="${1#*=}"
        EXPORT_BINARIES_ARG="--export-binaries=${export_path}"
        echo "Using EXPORT_BINARIES_ARG $EXPORT_BINARIES_ARG"
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

TST_COMMIT=${COMMIT:?"Missing arg #1 to specify a COMMIT"}
TST_REGISTRY=${REGISTRY:?"Missing arg #2 to specify target container registry"}

BUILD_IMAGE_TAG=$COMMIT

do_clone "$BRANCH" "./hive-${COMMIT}" https://gitlab.syncad.com/hive/hive.git "$COMMIT"

"$SCRIPTSDIR/ci-helpers/build_instance.sh" "${BUILD_IMAGE_TAG}" "./hive-${COMMIT}" "${REGISTRY}" ${EXPORT_BINARIES_ARG} ${NETWORK_TYPE_ARG}



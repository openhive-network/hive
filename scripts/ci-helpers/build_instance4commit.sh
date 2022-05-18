#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."
SRCROOTDIR="$SCRIPTSDIR/.."

LOG_FILE=build_instance4commit.log
source "$SCRIPTSDIR/common.sh"

COMMIT=""

REGISTRY=""

BRANCH="master"

NETWORK_TYPE_ARG=""

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
        NETWORK_TYPE_ARG="--network-type=${type}"
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

BUILD_IMAGE_TAG=$COMMIT

do_clone "$BRANCH" "./hive-${COMMIT}" https://gitlab.syncad.com/hive/hive.git "$COMMIT"

"$SCRIPTSDIR/ci-helpers/build_instance.sh" "${BUILD_IMAGE_TAG}" "${REGISTRY}" ${NETWORK_TYPE_ARG}



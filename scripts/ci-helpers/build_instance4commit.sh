#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 || exit 1; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."

export LOG_FILE=build_instance4commit.log
# shellcheck source=../common.sh
source "$SCRIPTSDIR/common.sh"

COMMIT=""

REGISTRY=""

BRANCH="master"

NETWORK_TYPE_ARG=""
EXPORT_BINARIES_ARG=""
BUILD_IMAGE_TAG=""

print_help () {
cat <<-EOF
  Usage: $0 <commit> <registry_url> [OPTION[=VALUE]]...

  Builds Docker image containing Hived installation built from specified COMMIT
  OPTIONS:
    --network-type=TYPE       Type of blockchain network supported by the built hived binary. Allowed values: mainnet, testnet, mirrornet.
    --export-binaries=PATH    Path where binaries shall be exported from the built image.
    --image-tag=TAG           Image tag. Defaults to short commit hash
    --help,-h,-?              Displays this help screen and exits
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --network-type=*)
        type="${1#*=}"
        NETWORK_TYPE_ARG="--network-type=${type}"
        ;;
    --export-binaries=*)
        export_path="${1#*=}"
        EXPORT_BINARIES_ARG="--export-binaries=${export_path}"
        ;;
    --image-tag=*)
        BUILD_IMAGE_TAG="${1#*=}"
        ;;
    --help|-h|-?)
        print_help
        exit 0
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

_TST_COMMIT=${COMMIT:?"Missing arg #1 to specify a COMMIT"}
_TST_REGISTRY=${REGISTRY:?"Missing arg #2 to specify target container registry"}

do_clone "$BRANCH" "./hive-${COMMIT}" https://gitlab.syncad.com/hive/hive.git "$COMMIT"

if [[ -z "$BUILD_IMAGE_TAG" ]]; then
  pushd "./hive-${COMMIT}" || exit 1
  BUILD_IMAGE_TAG=$(git rev-parse --short "$COMMIT")
  popd || exit 1
fi

# Use the build_instance.sh script from the new source root to avoid path resolution issues
pushd "./hive-${COMMIT}" || exit 1
"scripts/ci-helpers/build_instance.sh" "${BUILD_IMAGE_TAG}" "$(pwd)" "${REGISTRY}" "${NETWORK_TYPE_ARG}" "${EXPORT_BINARIES_ARG}"
popd || exit 1
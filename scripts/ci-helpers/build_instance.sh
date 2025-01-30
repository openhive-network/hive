#! /bin/bash
set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."

export LOG_FILE=build_instance.log
# shellcheck source=../common.sh
source "$SCRIPTSDIR/common.sh"

BUILD_IMAGE_TAG=""
REGISTRY=""
SRCROOTDIR=""

IMAGE_TAG_PREFIX=""

BUILD_HIVE_TESTNET=OFF
HIVE_CONVERTER_BUILD=OFF

EXPORT_PATH=""

print_help () {
cat <<-EOF
  Usage: $0 <image_tag> <src_dir> <registry_url> [OPTION[=VALUE]]...

  Builds docker image containing Hived installation
  OPTIONS:
      --network-type=TYPE       Allows to specify type of blockchain network supported by built hived. Allowed values: mainnet, testnet, mirrornet
      --export-binaries=PATH    Allows to specify a path where binaries shall be exported from built image.
      --help|-h|-?              Display this help screen and exit
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --network-type=*)
        type="${1#*=}"

        case $type in
          "testnet"*)
            BUILD_HIVE_TESTNET=ON
            IMAGE_TAG_PREFIX=testnet
            ;;
          "mirrornet"*)
            BUILD_HIVE_TESTNET=OFF
            HIVE_CONVERTER_BUILD=ON
            IMAGE_TAG_PREFIX=mirrornet
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
    --export-binaries=*)
        arg="${1#*=}"
        EXPORT_PATH="$arg"
        ;;
    --help|-h|-?)
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

_TST_IMGTAG=${BUILD_IMAGE_TAG:?"Missing arg #1 to specify built image tag"}
_TST_SRCDIR=${SRCROOTDIR:?"Missing arg #2 to specify source directory"}
_TST_REGISTRY=${REGISTRY:?"Missing arg #3 to specify target container registry"}

echo "Moving into source root directory: ${SRCROOTDIR}"

pushd "$SRCROOTDIR"
#pwd

# Extract relative HIVE_SUBDIR and absolute root SOURCE_DIR containing gitdir data automatically
HIVE_ROOT=$(realpath "${SCRIPTSDIR}/..")
SOURCE_DIR=$(git rev-parse --show-superproject-working-tree --show-toplevel | head -1)
HIVE_SUBDIR=$(realpath --relative-base="$SOURCE_DIR" "$HIVE_ROOT")

export DOCKER_BUILDKIT=1
BUILD_TIME="$(date -uIseconds)"
GIT_COMMIT_SHA="$(git rev-parse HEAD || true)"
if [ -z "$GIT_COMMIT_SHA" ]; then
  GIT_COMMIT_SHA="[unknown]"
fi

GIT_CURRENT_BRANCH="$(git branch --show-current || true)"
if [ -z "$GIT_CURRENT_BRANCH" ]; then
  GIT_CURRENT_BRANCH="$(git describe --abbrev=0 --all | sed 's/^.*\///' || true)"
  if [ -z "$GIT_CURRENT_BRANCH" ]; then
    GIT_CURRENT_BRANCH="[unknown]"
  fi
fi


GIT_LAST_LOG_MESSAGE="$(git log -1 --pretty=%B || true)"
if [ -z "$GIT_LAST_LOG_MESSAGE" ]; then
  GIT_LAST_LOG_MESSAGE="[unknown]"
fi

GIT_LAST_COMMITTER="$(git log -1 --pretty="%an <%ae>" || true)"
if [ -z "$GIT_LAST_COMMITTER" ]; then
  GIT_LAST_COMMITTER="[unknown]"
fi

GIT_LAST_COMMIT_DATE="$(git log -1 --pretty="%aI" || true)"
if [ -z "$GIT_LAST_COMMIT_DATE" ]; then
  GIT_LAST_COMMIT_DATE="[unknown]"
fi

echo -e "\nBuilding base instance image...\n"

docker build --target=base \
  --build-arg CI_REGISTRY_IMAGE="$REGISTRY/" \
  --build-arg BUILD_HIVE_TESTNET="$BUILD_HIVE_TESTNET" \
  --build-arg HIVE_CONVERTER_BUILD="$HIVE_CONVERTER_BUILD" \
  --build-arg BUILD_IMAGE_TAG="$BUILD_IMAGE_TAG" \
  --build-arg BUILD_TIME="$BUILD_TIME" \
  --build-arg GIT_COMMIT_SHA="$GIT_COMMIT_SHA" \
  --build-arg GIT_CURRENT_BRANCH="$GIT_CURRENT_BRANCH" \
  --build-arg GIT_LAST_LOG_MESSAGE="$GIT_LAST_LOG_MESSAGE" \
  --build-arg GIT_LAST_COMMITTER="$GIT_LAST_COMMITTER" \
  --build-arg GIT_LAST_COMMIT_DATE="$GIT_LAST_COMMIT_DATE" \
  --build-arg HIVE_SUBDIR="$HIVE_SUBDIR" \
  --build-arg IMAGE_TAG_PREFIX="${IMAGE_TAG_PREFIX:+$IMAGE_TAG_PREFIX-}" \
  --tag "${REGISTRY}/${IMAGE_TAG_PREFIX:+$IMAGE_TAG_PREFIX-}base:${BUILD_IMAGE_TAG}" \
  --file Dockerfile "$SOURCE_DIR"

echo -e "\nDone!\nBuilding instance image...\n"

docker build --target=instance \
  --build-arg CI_REGISTRY_IMAGE="$REGISTRY/" \
  --build-arg BUILD_HIVE_TESTNET=$BUILD_HIVE_TESTNET \
  --build-arg HIVE_CONVERTER_BUILD=$HIVE_CONVERTER_BUILD \
  --build-arg BUILD_IMAGE_TAG="$BUILD_IMAGE_TAG" \
  --build-arg BUILD_TIME="$BUILD_TIME" \
  --build-arg GIT_COMMIT_SHA="$GIT_COMMIT_SHA" \
  --build-arg GIT_CURRENT_BRANCH="$GIT_CURRENT_BRANCH" \
  --build-arg GIT_LAST_LOG_MESSAGE="$GIT_LAST_LOG_MESSAGE" \
  --build-arg GIT_LAST_COMMITTER="$GIT_LAST_COMMITTER" \
  --build-arg GIT_LAST_COMMIT_DATE="$GIT_LAST_COMMIT_DATE" \
  --build-arg HIVE_SUBDIR="$HIVE_SUBDIR" \
  --build-arg IMAGE_TAG_PREFIX="${IMAGE_TAG_PREFIX:+$IMAGE_TAG_PREFIX-}" \
  --tag "${REGISTRY}${IMAGE_TAG_PREFIX:+/$IMAGE_TAG_PREFIX}:${BUILD_IMAGE_TAG}" \
  --file Dockerfile "$SOURCE_DIR"

echo -e "\nDone!\nBuilding minimal instance image...\n"

docker build --target=minimal \
  --build-arg CI_REGISTRY_IMAGE="$REGISTRY/" \
  --build-arg BUILD_HIVE_TESTNET=$BUILD_HIVE_TESTNET \
  --build-arg HIVE_CONVERTER_BUILD=$HIVE_CONVERTER_BUILD \
  --build-arg BUILD_IMAGE_TAG="$BUILD_IMAGE_TAG" \
  --build-arg BUILD_TIME="$BUILD_TIME" \
  --build-arg GIT_COMMIT_SHA="$GIT_COMMIT_SHA" \
  --build-arg GIT_CURRENT_BRANCH="$GIT_CURRENT_BRANCH" \
  --build-arg GIT_LAST_LOG_MESSAGE="$GIT_LAST_LOG_MESSAGE" \
  --build-arg GIT_LAST_COMMITTER="$GIT_LAST_COMMITTER" \
  --build-arg GIT_LAST_COMMIT_DATE="$GIT_LAST_COMMIT_DATE" \
  --build-arg HIVE_SUBDIR="$HIVE_SUBDIR" \
  --build-arg IMAGE_TAG_PREFIX="${IMAGE_TAG_PREFIX:+$IMAGE_TAG_PREFIX-}" \
  --tag "${REGISTRY}/${IMAGE_TAG_PREFIX:+$IMAGE_TAG_PREFIX-}minimal:${BUILD_IMAGE_TAG}" \
  --file Dockerfile "$SOURCE_DIR"

echo -e "\nDone!\n"

popd

if [ -n "${EXPORT_PATH}" ];
then
  "$SCRIPTPATH/export-data-from-docker-image.sh" "${REGISTRY}/${IMAGE_TAG_PREFIX:+$IMAGE_TAG_PREFIX-}base:${BUILD_IMAGE_TAG}" "${EXPORT_PATH}"
fi


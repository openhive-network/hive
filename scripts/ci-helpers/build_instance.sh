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

  ENVIRONMENT VARIABLES:
      USE_BUILDX                Set to 'false' to disable BuildKit registry caching (default: true)
      SCCACHE_REDIS             Redis URL for sccache distributed compilation caching
      POSTGRES_VERSION          PostgreSQL major version to build against (e.g., 17, 18). Passed as Docker build-arg.
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

# Configure BuildKit registry caching
# Cache repository uses a /cache subpath in the registry
CACHE_REPO="${REGISTRY}/cache"
if [[ -n "$IMAGE_TAG_PREFIX" ]]; then
    CACHE_KEY="${IMAGE_TAG_PREFIX}-build"
else
    CACHE_KEY="mainnet-build"
fi

# Separate cache keys per PostgreSQL version to avoid mixing PG17/PG18 build caches
if [[ -n "${POSTGRES_VERSION:-}" ]]; then
    CACHE_KEY="${CACHE_KEY}-pg${POSTGRES_VERSION}"
fi

# Build POSTGRES_VERSION build-arg if set
PG_BUILD_ARG=""
if [[ -n "${POSTGRES_VERSION:-}" ]]; then
    PG_BUILD_ARG="--build-arg POSTGRES_VERSION=${POSTGRES_VERSION}"
fi

# Build cache arguments for buildx
# Pull from both old refs (build + instance) for smooth transition, write to single ref with mode=max
CACHE_FROM_BUILD="type=registry,ref=${CACHE_REPO}:${CACHE_KEY}"
CACHE_FROM_INSTANCE="type=registry,ref=${CACHE_REPO}:${CACHE_KEY}-instance"
CACHE_TO="type=registry,ref=${CACHE_REPO}:${CACHE_KEY},mode=max,compression=zstd,force-compression=false"

echo -e "Building Docker instance image (includes build stage)...\n"

# Single buildx invocation builds both build and instance stages.
# BuildKit resolves the build stage from the Dockerfile directly (no registry round-trip).
# mode=max caches all layers from all stages for future builds.
if docker buildx version &>/dev/null && [[ "${USE_BUILDX:-true}" != "false" ]]; then
    echo "Using docker buildx with registry caching (cache key: ${CACHE_KEY})..."
    docker buildx build --provenance=false --progress=plain --target=instance \
      --cache-from="$CACHE_FROM_BUILD" \
      --cache-from="$CACHE_FROM_INSTANCE" \
      --cache-to="$CACHE_TO" \
      --build-arg CI_REGISTRY_IMAGE="$REGISTRY/" \
      --build-arg BUILD_HIVE_TESTNET=$BUILD_HIVE_TESTNET \
      --build-arg HIVE_CONVERTER_BUILD=$HIVE_CONVERTER_BUILD \
      --build-arg BUILD_IMAGE_TAG="$BUILD_IMAGE_TAG" \
      --build-arg HIVE_SUBDIR="$HIVE_SUBDIR" \
      --build-arg IMAGE_TAG_PREFIX="${IMAGE_TAG_PREFIX:+$IMAGE_TAG_PREFIX-}" \
      --build-arg SCCACHE_REDIS="${SCCACHE_REDIS:-}" \
      --build-arg BUILD_TIME="$BUILD_TIME" \
      --build-arg GIT_COMMIT_SHA="$GIT_COMMIT_SHA" \
      --build-arg GIT_CURRENT_BRANCH="$GIT_CURRENT_BRANCH" \
      --build-arg GIT_LAST_LOG_MESSAGE="$GIT_LAST_LOG_MESSAGE" \
      --build-arg GIT_LAST_COMMITTER="$GIT_LAST_COMMITTER" \
      --build-arg GIT_LAST_COMMIT_DATE="$GIT_LAST_COMMIT_DATE" \
      ${PG_BUILD_ARG:+$PG_BUILD_ARG} \
      --tag "${REGISTRY}${IMAGE_TAG_PREFIX:+/$IMAGE_TAG_PREFIX}:${BUILD_IMAGE_TAG}" \
      --load \
      --file Dockerfile "$SOURCE_DIR"
else
    echo "Buildx not available or disabled, using docker build..."
    docker build --provenance=false --target=instance \
      --build-arg CI_REGISTRY_IMAGE="$REGISTRY/" \
      --build-arg BUILD_HIVE_TESTNET=$BUILD_HIVE_TESTNET \
      --build-arg HIVE_CONVERTER_BUILD=$HIVE_CONVERTER_BUILD \
      --build-arg BUILD_IMAGE_TAG="$BUILD_IMAGE_TAG" \
      --build-arg HIVE_SUBDIR="$HIVE_SUBDIR" \
      --build-arg IMAGE_TAG_PREFIX="${IMAGE_TAG_PREFIX:+$IMAGE_TAG_PREFIX-}" \
      --build-arg SCCACHE_REDIS="${SCCACHE_REDIS:-}" \
      --build-arg BUILD_TIME="$BUILD_TIME" \
      --build-arg GIT_COMMIT_SHA="$GIT_COMMIT_SHA" \
      --build-arg GIT_CURRENT_BRANCH="$GIT_CURRENT_BRANCH" \
      --build-arg GIT_LAST_LOG_MESSAGE="$GIT_LAST_LOG_MESSAGE" \
      --build-arg GIT_LAST_COMMITTER="$GIT_LAST_COMMITTER" \
      --build-arg GIT_LAST_COMMIT_DATE="$GIT_LAST_COMMIT_DATE" \
      ${PG_BUILD_ARG:+$PG_BUILD_ARG} \
      --tag "${REGISTRY}${IMAGE_TAG_PREFIX:+/$IMAGE_TAG_PREFIX}:${BUILD_IMAGE_TAG}" \
      --file Dockerfile "$SOURCE_DIR"
fi

echo -e "\nDone!\n"

popd

if [ -n "${EXPORT_PATH}" ];
then
  "$SCRIPTPATH/export-data-from-docker-image.sh" "${REGISTRY}${IMAGE_TAG_PREFIX:+/$IMAGE_TAG_PREFIX}:${BUILD_IMAGE_TAG}" "${EXPORT_PATH}"
fi


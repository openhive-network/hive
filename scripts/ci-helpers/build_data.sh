#! /bin/bash

SCRIPTPATH=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPTPATH")
SCRIPTSDIR="$SCRIPTPATH/.."

# shellcheck disable=SC2034 
LOG_FILE=build_data.log
# shellcheck source=../common.sh
source "$SCRIPTSDIR/common.sh"

BUILD_IMAGE_TAG=${1:?"Missing argument #1: build image tag"}
shift
SRCROOTDIR=${1:?"Missing argument #2: source directory"}
shift
REGISTRY=${1:?"Missing argument #3: target image registry"}
shift

# Supplement a registry path by trailing slash (if needed)
[[ "${REGISTRY}" != */ ]] && REGISTRY="${REGISTRY}/"

BUILD_HIVE_TESTNET=OFF
HIVE_CONVERTER_BUILD=OFF

if [ -n "${CI:-}" ];
then
  BUILDKIT_CACHE_PATH=${1:?"Missing argument #3: Docker BuildKit cache path"}
  shift
  BINARY_CACHE_PATH=${1:?"Missing argument #4: binary cache path"}
  shift

  CACHE_REF="${CI_PROJECT_DIR}/${BUILDKIT_CACHE_PATH}"
  export CACHE_REF
  export REGISTRY
  export BUILD_HIVE_TESTNET
  export HIVE_CONVERTER_BUILD
  export BUILD_IMAGE_TAG
  export IMAGE_TAG_PREFIX

  pushd "$SRCROOTDIR" || exit 1

  docker buildx bake --progress=plain --load data-ci
  
  du -h "$CACHE_REF"
  docker image ls

  popd || exit 1

  set -x
  BUILDER_NAME=$(docker buildx inspect | head -n 1 | awk '{print $2}')
  echo "BUILDER_NAME=${BUILDER_NAME}"
  docker buildx use default
  "$SCRIPTPATH/export-binaries.sh" "${REGISTRY}data:data-${BUILD_IMAGE_TAG}" "${BINARY_CACHE_PATH}"
  docker buildx use "$BUILDER_NAME"
  set +x
else
  "$SCRIPTSDIR/ci-helpers/build_instance.sh" "${BUILD_IMAGE_TAG}" "${SRCROOTDIR}" "${REGISTRY}" "$@"

  pushd "$SRCROOTDIR" || exit 1
  echo "Instance image built. Attempting to build a data image basing on it..."
  docker buildx build --target=data  --progress=plain \
    --build-arg CI_REGISTRY_IMAGE="$REGISTRY" \
    --build-arg BUILD_HIVE_TESTNET=$BUILD_HIVE_TESTNET \
    --build-arg HIVE_CONVERTER_BUILD=$HIVE_CONVERTER_BUILD \
    --build-arg BUILD_IMAGE_TAG="$BUILD_IMAGE_TAG" \
    --tag "${REGISTRY}data:data-${BUILD_IMAGE_TAG}" -f Dockerfile .
  popd || exit 1  
fi

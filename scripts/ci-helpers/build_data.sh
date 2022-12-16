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
BRANCH_NAME=${1:?"Missing argument #4: branch to get buildkit cache for"}
shift

# Supplement a registry path by trailing slash (if needed)
[[ "${REGISTRY}" != */ ]] && REGISTRY="${REGISTRY}/"

BUILD_HIVE_TESTNET=OFF
HIVE_CONVERTER_BUILD=OFF

"$SCRIPTSDIR/ci-helpers/build_instance.sh" "${BUILD_IMAGE_TAG}" "${SRCROOTDIR}" "${REGISTRY}" "--cache-path=$BRANCH_NAME" "$@"

echo "Instance image built. Attempting to build a data image basing on it..."

pushd "$SRCROOTDIR" || exit 1

docker buildx build --target=data  --progress=plain --load \
  --build-arg CI_REGISTRY_IMAGE="$REGISTRY" \
  --build-arg BUILD_HIVE_TESTNET=$BUILD_HIVE_TESTNET \
  --build-arg HIVE_CONVERTER_BUILD=$HIVE_CONVERTER_BUILD \
  --build-arg BUILD_IMAGE_TAG="$BUILD_IMAGE_TAG" \
  --cache-from "type=registry,ref=${REGISTRY}data:data-$BRANCH_NAME" \
  --cache-to "type=registry,mode=max,ref=${REGISTRY}data:data-$BRANCH_NAME" \
  --tag "${REGISTRY}data:data-${BUILD_IMAGE_TAG}" -f Dockerfile .

popd || exit 1

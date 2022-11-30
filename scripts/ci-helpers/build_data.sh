#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."

LOG_FILE=build_data.log
source "$SCRIPTSDIR/common.sh"

BUILD_IMAGE_TAG=${1:?"Missing arg #1 to specify built image tag"}
shift
SRCROOTDIR=${1:?Missing arg #2 to specify source directory}
shift
REGISTRY=${1:?"Missing arg #3 to specify target container registry"}
shift 

# Supplement a registry path by trailing slash (if needed)
[[ "${REGISTRY}" != */ ]] && REGISTRY="${REGISTRY}/"

BUILD_HIVE_TESTNET=OFF
HIVE_CONVERTER_BUILD=OFF

"$SCRIPTSDIR/ci-helpers/build_instance.sh" "${BUILD_IMAGE_TAG}" "${SRCROOTDIR}" "${REGISTRY}" "$@"

echo "Instance image built. Attempting to build a data image basing on it..."

pushd "$SRCROOTDIR"

export DOCKER_BUILDKIT=1

docker build --target=data \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY \
  --build-arg BUILD_HIVE_TESTNET=$BUILD_HIVE_TESTNET \
  --build-arg HIVE_CONVERTER_BUILD=$HIVE_CONVERTER_BUILD \
  --build-arg BUILD_IMAGE_TAG=$BUILD_IMAGE_TAG \
  -t ${REGISTRY}data:data-${BUILD_IMAGE_TAG} -f Dockerfile .

popd

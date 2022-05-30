#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."

LOG_FILE=build_data.log
source "$SCRIPTSDIR/common.sh"

BUILD_IMAGE_TAG=${1:?"Missing arg #1 to specify built image tag"}
SRCROOTDIR=${2:?Missing arg #2 to specify source directory}
REGISTRY=${3:?"Missing arg #3 to specify target container registry"}

BLOCK_LOG_SUFFIX="-5m"

"$SCRIPTSDIR/ci-helpers/build_instance.sh" "${BUILD_IMAGE_TAG}" "${SRCROOTDIR}" "${REGISTRY}" "${BLOCK_LOG_SUFFIX}"

pushd "$SRCROOTDIR"

docker build --target=data \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg BLOCK_LOG_SUFFIX="-5m" \
  --build-arg BUILD_IMAGE_TAG=$BUILD_IMAGE_TAG -t ${REGISTRY}data:data-${BUILD_IMAGE_TAG} -f Dockerfile .

popd

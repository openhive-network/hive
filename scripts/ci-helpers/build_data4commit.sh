#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."
SRCROOTDIR="$SCRIPTSDIR/.."

LOG_FILE=build_data4commit.log
source "$SCRIPTSDIR/common.sh"

COMMIT="$1"

REGISTRY=${2:?"Missing arg 2 for REGISTRY"}

export BLOCK_LOG_SUFFIX="-5m"

CI_IMAGE_TAG=:ubuntu20.04-3
BUILD_IMAGE_TAG=:$COMMIT

pushd "$SRCROOTDIR"
pwd

"$SCRIPTSDIR/ci-helpers/build_instance4commit.sh" $COMMIT $REGISTRY

docker build --target=data \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG --build-arg BLOCK_LOG_SUFFIX="-5m" \
  --build-arg COMMIT=$COMMIT --build-arg BUILD_IMAGE_TAG=$BUILD_IMAGE_TAG -t ${REGISTRY}data$BUILD_IMAGE_TAG -f Dockerfile .

popd

#! /bin/bash

REGISTRY=${1:-registry.gitlab.syncad.com/hive/hive/}
CI_IMAGE_TAG=:ubuntu20.04-6

export DOCKER_BUILDKIT=1

docker build --target=runtime \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  -t ${REGISTRY}runtime$CI_IMAGE_TAG -f Dockerfile .


docker build --target=ci-base-image \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  -t ${REGISTRY}ci-base-image$CI_IMAGE_TAG -f Dockerfile .

docker build --target=ci-base-image-5m \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  -t ${REGISTRY}ci-base-image-5m$CI_IMAGE_TAG -f Dockerfile .

docker build --target=block_log_5m_source \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  -t ${REGISTRY}block_log_5m_source$CI_IMAGE_TAG -f Dockerfile .

docker build --target=block_log_mirrornet_5m_source \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  --build-arg BUILD_HIVE_TESTNET=OFF --build-arg HIVE_CONVERTER_BUILD=ON \
  -t ${REGISTRY}block_log_mirrornet_5m_source$CI_IMAGE_TAG -f Dockerfile .

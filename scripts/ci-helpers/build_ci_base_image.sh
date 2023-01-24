#! /bin/bash

REGISTRY=${1:-registry.gitlab.syncad.com/hive/hive/}
CI_IMAGE_TAG=:ubuntu22.04-1

docker buildx build --progress=plain --target=runtime \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  -t ${REGISTRY}runtime$CI_IMAGE_TAG -f Dockerfile .


docker buildx build --progress=plain --target=ci-base-image \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  -t ${REGISTRY}ci-base-image$CI_IMAGE_TAG -f Dockerfile .

docker buildx build --progress=plain --target=ci-base-image-5m \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  -t ${REGISTRY}ci-base-image-5m$CI_IMAGE_TAG -f Dockerfile .

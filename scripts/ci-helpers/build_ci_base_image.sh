#! /bin/bash

REGISTRY=${1:-registry.gitlab.syncad.com/hive/hive/}
CI_IMAGE_TAG=ubuntu24.04-1

docker build --progress=plain --target=runtime \
  --build-arg CI_REGISTRY_IMAGE="$REGISTRY" --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  --build-arg BUILD_IMAGE_TAG=$CI_IMAGE_TAG \
  --tag "${REGISTRY}runtime:$CI_IMAGE_TAG" --file Dockerfile .


docker build --progress=plain --target=minimal-runtime \
  --build-arg CI_REGISTRY_IMAGE="$REGISTRY" --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  --build-arg BUILD_IMAGE_TAG=$CI_IMAGE_TAG \
  --tag "${REGISTRY}minimal-runtime:$CI_IMAGE_TAG" --file Dockerfile .


docker build --progress=plain --target=ci-base-image \
  --build-arg CI_REGISTRY_IMAGE="$REGISTRY" --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  --build-arg BUILD_IMAGE_TAG=$CI_IMAGE_TAG \
  --tag "${REGISTRY}ci-base-image:$CI_IMAGE_TAG" --file Dockerfile .

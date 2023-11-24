#! /bin/bash

REGISTRY=${1:-registry.gitlab.syncad.com/hive/hive/}
CI_IMAGE_TAG=:ubuntu22.04-9

docker buildx build --progress=plain --target=runtime \
  --build-arg CI_REGISTRY_IMAGE="$REGISTRY" --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  --tag "${REGISTRY}runtime$CI_IMAGE_TAG" --file Dockerfile .


docker buildx build --progress=plain --target=ci-base-image \
  --build-arg CI_REGISTRY_IMAGE="$REGISTRY" --build-arg CI_IMAGE_TAG=$CI_IMAGE_TAG \
  --tag "${REGISTRY}ci-base-image$CI_IMAGE_TAG" --file Dockerfile .

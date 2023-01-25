#! /bin/bash

[[ -z "$DOCKER_HUB_USER" ]] && echo "Variable DOCKER_HUB_USER must be set" && exit 1
[[ -z "$DOCKER_HUB_PASSWORD" ]] && echo "Variable DOCKER_HUB_PASSWORD must be set" && exit 1

set -e

docker login -u "$CI_REGISTRY_USER" -p "$CI_REGISTRY_PASSWORD" "$CI_REGISTRY"
docker login -u "$DOCKER_HUB_USER" -p "$DOCKER_HUB_PASSWORD"

pushd "$CI_PROJECT_DIR" || exit 1

docker buildx build --progress=plain --target=base_instance \
  --build-arg CI_REGISTRY_IMAGE="$CI_REGISTRY_IMAGE/" \
  --build-arg BUILD_HIVE_TESTNET="OFF" \
  --build-arg HIVE_CONVERTER_BUILD="OFF" \
  --build-arg BUILD_IMAGE_TAG="$CI_COMMIT_TAG" \
  --tag "${CI_REGISTRY_IMAGE}/base_instance:base_instance-${CI_COMMIT_TAG}" \
  --file Dockerfile .

docker buildx build --progress=plain --target=instance \
  --build-arg CI_REGISTRY_IMAGE="$CI_REGISTRY_IMAGE/" \
  --build-arg BUILD_HIVE_TESTNET="OFF" \
  --build-arg HIVE_CONVERTER_BUILD="OFF" \
  --build-arg BUILD_IMAGE_TAG="$CI_COMMIT_TAG" \
  --tag "${CI_REGISTRY_IMAGE}/instance:instance-${CI_COMMIT_TAG}" \
  --tag "hiveio/hive:${CI_COMMIT_TAG}" \
  --file Dockerfile .

docker push "${CI_REGISTRY_IMAGE}/instance:instance-${CI_COMMIT_TAG}"
docker push "hiveio/hive:${CI_COMMIT_TAG}"
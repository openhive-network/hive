#! /bin/bash

[[ -z "$DOCKER_HUB_USER" ]] && echo "Variable DOCKER_HUB_USER must be set" && exit 1
[[ -z "$DOCKER_HUB_PASSWORD" ]] && echo "Variable DOCKER_HUB_PASSWORD must be set" && exit 1
[[ -z "$HIVED_IMAGE_NAME" ]] && echo "Variable HIVED_IMAGE_NAME must be set" && exit 1
[[ -z "$HIVED_IMAGE_NAME_COMMIT" ]] && echo "Variable HIVED_IMAGE_NAME_COMMIT must be set" && exit 1

set -e

docker login -u "$CI_REGISTRY_USER" -p "$CI_REGISTRY_PASSWORD" "$CI_REGISTRY"

# Pull base_instance image
docker pull "$HIVED_IMAGE_NAME"

pushd "$CI_PROJECT_DIR"

# Download Dockerfile used to build the base_image
wget "https://gitlab.syncad.com/hive/hive/-/blob/$HIVED_IMAGE_NAME_COMMIT/Dockerfile" -O "Dockerfile.$HIVED_IMAGE_NAME_COMMIT"

# Build the instance image
TAG="${CI_REGISTRY_IMAGE}/instance:instance-${HIVED_IMAGE_NAME_COMMIT}"
docker buildx build --progress=plain --target=instance \
  --build-arg CI_REGISTRY_IMAGE="$CI_REGISTRY_IMAGE/" \
  --build-arg BUILD_HIVE_TESTNET="OFF" \
  --build-arg HIVE_CONVERTER_BUILD="OFF" \
  --build-arg BUILD_IMAGE_TAG="$HIVED_IMAGE_NAME_COMMIT" \
  --tag "$TAG" \
  --file "Dockerfile.$HIVED_IMAGE_NAME_COMMIT" .

docker push "$TAG"  

popd
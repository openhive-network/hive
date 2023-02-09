#! /bin/bash

[[ -z "$DOCKER_HUB_USER" ]] && echo "Variable DOCKER_HUB_USER must be set" && exit 1
[[ -z "$DOCKER_HUB_PASSWORD" ]] && echo "Variable DOCKER_HUB_PASSWORD must be set" && exit 1
[[ -z "$HIVED_IMAGE_NAME_COMMIT" ]] && echo "Variable HIVED_IMAGE_NAME_COMMIT must be set" && exit 1

set -e

docker login -u "$CI_REGISTRY_USER" -p "$CI_REGISTRY_PASSWORD" "$CI_REGISTRY"
docker login -u "$DOCKER_HUB_USER" -p "$DOCKER_HUB_PASSWORD"

# pull the instance image for appropriate commit
INSTANCE_TAG="${CI_REGISTRY_IMAGE}/instance:instance-${HIVED_IMAGE_NAME_COMMIT}"
docker pull "$INSTANCE_TAG"

# tag the instance image for release
docker tag "$INSTANCE_TAG" "${CI_REGISTRY_IMAGE}/instance:instance-${CI_COMMIT_TAG}"
docker tag "$INSTANCE_TAG" "hiveio/hive:${CI_COMMIT_TAG}"

docker images

docker push "${CI_REGISTRY_IMAGE}/instance:instance-${CI_COMMIT_TAG}"
docker push "hiveio/hive:${CI_COMMIT_TAG}"
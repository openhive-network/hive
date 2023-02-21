#! /bin/bash

[[ -z "$DOCKER_HUB_USER" ]] && echo "Variable DOCKER_HUB_USER must be set" && exit 1
[[ -z "$DOCKER_HUB_PASSWORD" ]] && echo "Variable DOCKER_HUB_PASSWORD must be set" && exit 1

set -e

docker login -u "$CI_REGISTRY_USER" -p "$CI_REGISTRY_PASSWORD" "$CI_REGISTRY"
docker login -u "$DOCKER_HUB_USER" -p "$DOCKER_HUB_PASSWORD"

pushd "$CI_PROJECT_DIR" || exit 1

# Build instance image
scripts/ci-helpers/build_instance.sh "$CI_COMMIT_TAG" "." "registry.gitlab.syncad.com/hive/hive"

# Tag instance image
docker tag "${CI_REGISTRY_IMAGE}/instance:instance-${CI_COMMIT_TAG}" "hiveio/hive:${CI_COMMIT_TAG}"

docker images

docker push "${CI_REGISTRY_IMAGE}/instance:instance-${CI_COMMIT_TAG}"
docker push "hiveio/hive:${CI_COMMIT_TAG}"

popd || exit 1
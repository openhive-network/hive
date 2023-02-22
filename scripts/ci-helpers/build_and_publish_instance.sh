#! /bin/bash

[[ -z "$CI_REGISTRY_USER" ]] && echo "Variable CI_REGISTRY_USER must be set" && exit 1
[[ -z "$CI_REGISTRY_PASSWORD" ]] && echo "Variable CI_REGISTRY_PASSWORD must be set" && exit 1
[[ -z "$CI_REGISTRY" ]] && echo "Variable CI_REGISTRY must be set" && exit 1
[[ -z "$CI_COMMIT_TAG" ]] && echo "Variable CI_COMMIT_TAG must be set" && exit 1
[[ -z "$CI_REGISTRY_IMAGE" ]] && echo "Variable CI_REGISTRY_IMAGE must be set" && exit 1
[[ -z "$DOCKER_HUB_USER" ]] && echo "Variable DOCKER_HUB_USER must be set" && exit 1
[[ -z "$DOCKER_HUB_PASSWORD" ]] && echo "Variable DOCKER_HUB_PASSWORD must be set" && exit 1

SCRIPT_DIR=$(dirname "$(realpath "$0")")
SRC_DIR="$SCRIPT_DIR/../.."

set -e

docker login -u "$CI_REGISTRY_USER" -p "$CI_REGISTRY_PASSWORD" "$CI_REGISTRY"
docker login -u "$DOCKER_HUB_USER" -p "$DOCKER_HUB_PASSWORD"

# Build instance image
"$SRC_DIR/scripts/ci-helpers/build_instance.sh" "$CI_COMMIT_TAG" "$SRC_DIR" "${CI_REGISTRY_IMAGE}"

# Tag instance image
docker tag "${CI_REGISTRY_IMAGE}/instance:instance-${CI_COMMIT_TAG}" "hiveio/hive:${CI_COMMIT_TAG}"

docker images

# Push instance images
docker push "${CI_REGISTRY_IMAGE}/instance:instance-${CI_COMMIT_TAG}"
docker push "hiveio/hive:${CI_COMMIT_TAG}"
#! /bin/bash

SRC_DIR_OFFSET="${SRC_DIR_OFFSET:-../..}" #This needs to be defined as a variable that can be overridden if Hive is a submodule
SCRIPT_DIR=$(dirname "$(realpath "$0")")
SRC_DIR="$SCRIPT_DIR/$SRC_DIR_OFFSET"

print_help () {
    echo "Usage: $0 OPTION[=VALUE]..."
    echo
    echo "Script for building Docker image of Hive instance and pushingit to GitLab registry and to Docker Hub"
    echo "All options (except '--help') are required"
    echo "OPTIONS:"
    echo "  --registry-user=USERNAME   Docker registry user"
    echo "  --registry-password=PASS   Docker registry user"
    echo "  --registry=URL             Docker registry URL, eg. 'registry.gitlab.syncad.com'"
    echo "  --image-tag=TAG            Docker image tag, in CI the same as Git tag"
    echo "  --image-name-prefix=PREFIX Docker image name prefix, eg. 'registry.gitlab.syncad.com/hive/hive'"
    echo "  --docker-hub-user=USERNAME Docker Hub username"
    echo "  --docker-hub-password=PASS Docker Hub password"
    echo "  -?/--help                  Display this help screen and exit"
    echo
}

set -e

while [ $# -gt 0 ]; do
  case "$1" in
    --registry-user=*)
        arg="${1#*=}"
        CI_REGISTRY_USER="$arg"
        ;;
    --registry-password=*)
        arg="${1#*=}"
        CI_REGISTRY_PASSWORD="$arg"
        ;;
    --registry=*)
        arg="${1#*=}"
        CI_REGISTRY="$arg"
        ;;
    --image-tag=*)
        arg="${1#*=}"
        CI_COMMIT_TAG="$arg"
        ;;
    --image-name-prefix=*)
        arg="${1#*=}"
        CI_REGISTRY_IMAGE="$arg"
        ;;
    --docker-hub-user=*)
        arg="${1#*=}"
        DOCKER_HUB_USER="$arg"
        ;;
    --docker-hub-password=*)
        arg="${1#*=}"
        DOCKER_HUB_PASSWORD="$arg"
        ;;
    --help)
        print_help
        exit 0
        ;;
    -?)
        print_help
        exit 0
        ;;
    *)
        echo "ERROR: '$1' is not a valid option/positional argument"
        echo
        print_help
        exit 2
        ;;
    esac
    shift
done

[[ -z "$CI_REGISTRY_USER" ]] && echo "Option '--registry-user' must be set" && print_help && exit 1
[[ -z "$CI_REGISTRY_PASSWORD" ]] && echo "Option '--registry-password' must be set" && print_help && exit 1
[[ -z "$CI_REGISTRY" ]] && echo "Option '--registry' must be set" && print_help && exit 1
[[ -z "$CI_COMMIT_TAG" ]] && echo "Option '--image-tag' must be set" && print_help && exit 1
[[ -z "$CI_REGISTRY_IMAGE" ]] && echo "Option '--image-name-prefix' must be set" && print_help && exit 1
[[ -z "$DOCKER_HUB_USER" ]] && echo "Option '--docker-hub-user' must be set" && print_help && exit 1
[[ -z "$DOCKER_HUB_PASSWORD" ]] && echo "Option '--docker-hub-password' must be set" && print_help && exit 1

CI_PROJECT_NAME="${CI_REGISTRY_IMAGE##*/}"

echo "Logging to Docker Hub and $CI_REGISTRY"
docker login -u "$CI_REGISTRY_USER" -p "$CI_REGISTRY_PASSWORD" "$CI_REGISTRY"
docker login -u "$DOCKER_HUB_USER" -p "$DOCKER_HUB_PASSWORD"

echo "Building an instance image in the source directory $SRC_DIR"
"$SRC_DIR/scripts/ci-helpers/build_instance.sh" "$CI_COMMIT_TAG" "$SRC_DIR" "$CI_REGISTRY_IMAGE"

echo "Tagging the image built in the previous step as hiveio/$CI_PROJECT_NAME:$CI_COMMIT_TAG"
docker tag "$CI_REGISTRY_IMAGE/instance:instance-$CI_COMMIT_TAG" "hiveio/$CI_PROJECT_NAME:$CI_COMMIT_TAG"

docker images

echo "Pushing instance images"
docker push "$CI_REGISTRY_IMAGE/instance:instance-$CI_COMMIT_TAG"
docker push "hiveio/$CI_PROJECT_NAME:$CI_COMMIT_TAG"
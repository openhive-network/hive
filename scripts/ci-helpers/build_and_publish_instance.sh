#! /bin/bash

SCRIPT_DIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 || exit 1; pwd -P )"
SRC_DIR="$SCRIPT_DIR/../.."

print_help () {
cat <<EOF
Usage: $0 OPTION[=VALUE]...

Script for building Docker image of Hive instance and pushing it to GitLab registry and to Docker Hub
All options (except '--help') are required
OPTIONS:
  --registry-user=USERNAME      Docker registry user
  --registry-password=PASS      Docker registry user
  --registry=URL                Docker registry URL, eg. 'registry.gitlab.syncad.com'
  --image-tag=TAG               Docker image tag, in CI the same as Git tag
  --image-name-prefix=PREFIX    Docker image name prefix, eg. 'registry.gitlab.syncad.com/hive/hive'
  --docker-hub-user=USERNAME    Docker Hub username
  --docker-hub-password=PASS    Docker Hub password
  --blog-registry-user=USERNAME Hive Blog registry username
  --blog-registry-password=PASS Hive Blog registry password
  -?/--help                     Display this help screen and exit

EOF
}

set -e

BUILD_INSTANCE_ADDITIONAL_ARGS=()

add_build_instance_arg() {
  local arg="$1"
  BUILD_INSTANCE_ADDITIONAL_ARGS+=("$arg")
}


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
    --blog-registry-user=*)
        arg="${1#*=}"
        BLOG_REGISTRY_USER="$arg"
        ;;
    --blog-registry-password=*)
        arg="${1#*=}"
        BLOG_REGISTRY_PASSWORD="$arg"
        ;;
    --build-instance-arg=*)
        arg="${1#*=}"
        add_build_instance_arg "${arg}"
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
[[ -z "$BLOG_REGISTRY_USER" ]] && echo "Option '--blog-registry-user' must be set" && print_help && exit 1
[[ -z "$BLOG_REGISTRY_PASSWORD" ]] && echo "Option '--blog-registry-password' must be set" && print_help && exit 1

CI_PROJECT_NAME="${CI_REGISTRY_IMAGE##*/}"

echo "Logging to Docker Hub, $CI_REGISTRY and registry.hive.blog"
docker login -u "$CI_REGISTRY_USER" --password-stdin "$CI_REGISTRY" <<< "$CI_REGISTRY_PASSWORD"
docker login -u "$DOCKER_HUB_USER" --password-stdin <<< "$DOCKER_HUB_PASSWORD"
docker login -u "$BLOG_REGISTRY_USER" --password-stdin "registry-upload.hive.blog" <<< "$BLOG_REGISTRY_PASSWORD"

echo "Building an instance image in the source directory $SRC_DIR"
"$SRC_DIR/scripts/ci-helpers/build_instance.sh" "$CI_COMMIT_TAG" "$SRC_DIR" "$CI_REGISTRY_IMAGE" "${BUILD_INSTANCE_ADDITIONAL_ARGS[@]}"

echo "Tagging the image built in the previous step as ${CI_REGISTRY_IMAGE}"
docker tag "${CI_REGISTRY_IMAGE}:${CI_COMMIT_TAG}" "${CI_REGISTRY_IMAGE}:${CI_COMMIT_TAG}"

HIVEIO_IMG="hiveio/${CI_PROJECT_NAME}:${CI_COMMIT_TAG}"

echo "Tagging the image built in the previous step as ${HIVEIO_IMG}"
docker tag "${CI_REGISTRY_IMAGE}:${CI_COMMIT_TAG}" "${HIVEIO_IMG}"

HIVEBLOG_IMG="registry-upload.hive.blog/${CI_PROJECT_NAME}:${CI_COMMIT_TAG}"

echo "Tagging the image built in the previous step as ${HIVEBLOG_IMG}"
docker tag "$CI_REGISTRY_IMAGE:$CI_COMMIT_TAG" "${HIVEBLOG_IMG}"

docker images

echo "Pushing instance images"
docker push "${CI_REGISTRY_IMAGE}:${CI_COMMIT_TAG}"
docker push "${HIVEIO_IMG}"
docker push "${HIVEBLOG_IMG}"

# Add another variable pointing image name to use it while deploying i.e. clive project
echo DOCKERHUB_IMAGE_NAME="hiveio/${CI_PROJECT_NAME}:$CI_COMMIT_TAG" >> docker_image_name.env

#! /bin/bash

SCRIPTPATH=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPTPATH")
SCRIPTSDIR="$SCRIPTPATH/.."

# shellcheck disable=SC2034 
LOG_FILE=build_data4commit.log

DIRECTORY_PREFIX=${DIRECTORY_PREFIX:-"hive-"}
GIT_REPOSITORY=${GIT_REPOSITORY:-"https://gitlab.syncad.com/hive/hive.git"}

# shellcheck source=../common.sh
source "$SCRIPTSDIR/common.sh"

COMMIT=${1:?"Missing argument #1: commit SHA"}
shift
REGISTRY=${1:?"Missing argument #2: target image registry"}
shift

BRANCH="master"

BUILD_IMAGE_TAG=$COMMIT

do_clone "$BRANCH" "./${DIRECTORY_PREFIX}${COMMIT}" "${GIT_REPOSITORY}" "$COMMIT"

if [ -n "${CI:-}" ];
then
    BUILDKIT_CACHE_PATH=${1:?"Missing argument #3: Docker BuildKit cache path"}
    shift
    "$SCRIPTSDIR/ci-helpers/build_data.sh" "$BUILD_IMAGE_TAG" "./${DIRECTORY_PREFIX}${COMMIT}" "$REGISTRY" "$BUILDKIT_CACHE_PATH" "$@"
else
    "$SCRIPTSDIR/ci-helpers/build_data.sh" "$BUILD_IMAGE_TAG" "./${DIRECTORY_PREFIX}${COMMIT}" "$REGISTRY" "$@"
fi

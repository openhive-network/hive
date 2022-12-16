#! /bin/bash

SCRIPTPATH=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPTPATH")
SCRIPTSDIR="$SCRIPTPATH/.."

# shellcheck disable=SC2034 
LOG_FILE=build_data4commit.log
# shellcheck source=../common.sh
source "$SCRIPTSDIR/common.sh"

COMMIT=${1:?"Missing argument #1: commit SHA"}
shift
REGISTRY=${1:?"Missing argument #2: target image registry"}
shift
BRANCH_NAME=${1:?"Missing argument #3: branch to get buildkit cache for"}
shift
BRANCH="master"

BUILD_IMAGE_TAG=$COMMIT

do_clone "$BRANCH" "./hive-${COMMIT}" https://gitlab.syncad.com/hive/hive.git "$COMMIT"

"$SCRIPTSDIR/ci-helpers/build_data.sh" "$BUILD_IMAGE_TAG" "./hive-${COMMIT}" "$REGISTRY" "$BRANCH_NAME" "$@"

